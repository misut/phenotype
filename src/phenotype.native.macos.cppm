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
import phenotype.material;
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

// Font cache: keyed by (family, weight, style, mono). Each slot owns
// a base 16pt CTFontRef; per-call sizing is layered on top via
// CTFontCreateCopyWithAttributes (matches the pre-FontSpec sans/mono
// pattern). Empty family + mono=false → Pretendard when available, then system sans;
// empty family + mono=true → user fixed-pitch.
struct FontCacheKey {
    std::string family;
    ::phenotype::FontWeight weight = ::phenotype::FontWeight::Regular;
    ::phenotype::FontStyle  style  = ::phenotype::FontStyle::Upright;
    bool                     mono   = false;
};

inline FontCacheKey default_text_font_key(bool mono) {
    FontCacheKey key{};
    key.mono = mono;
    if (!mono)
        key.family = ::phenotype::detail::g_app.theme.default_font_family;
    return key;
}

struct FontCacheKeyLess {
    bool operator()(FontCacheKey const& l, FontCacheKey const& r) const noexcept {
        if (l.family != r.family) return l.family < r.family;
        if (l.weight != r.weight)
            return static_cast<unsigned char>(l.weight)
                 < static_cast<unsigned char>(r.weight);
        if (l.style != r.style)
            return static_cast<unsigned char>(l.style)
                 < static_cast<unsigned char>(r.style);
        return static_cast<unsigned char>(l.mono)
             < static_cast<unsigned char>(r.mono);
    }
};

struct TextState {
    // Pre-warmed default faces — kept for the empty-family fast path
    // (most widget::text calls don't carry a custom family).
    CTFontRef sans = nullptr;
    CTFontRef mono = nullptr;
    // Resolved base 16pt fonts keyed by FontCacheKey. CFRetained so the
    // map owns the references; cleaned up on text_shutdown.
    std::map<FontCacheKey, CTFontRef, FontCacheKeyLess> cache;
    // Family names that previously failed to resolve — logged once each
    // and never re-attempted (the lookup is expensive on Core Text).
    std::set<std::string> missing_logged;
    // Process-registered TTF/OTF/TTC files — alias name → resolved
    // PostScript name. Populated by `register_font_file_macos`; consulted
    // by `acquire_base_font` BEFORE handing the raw FontSpec family to
    // CTFontCreateWithName, so callers can register a font under a
    // friendly alias and continue addressing it by that alias from
    // FontSpec.
    std::map<std::string, std::string> registered_aliases;
    bool initialized = false;
};

inline TextState g_text;

inline FontCacheKey font_key_from_spec(::phenotype::FontSpec const& font) {
    return FontCacheKey{
        std::string(font.family),
        font.weight,
        font.style,
        font.mono,
    };
}

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

inline SEL sel_control_accent_color() {
    static auto sel = sel_registerName("controlAccentColor");
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

inline SEL sel_style_mask() {
    static auto sel = sel_registerName("styleMask");
    return sel;
}

inline SEL sel_set_style_mask() {
    static auto sel = sel_registerName("setStyleMask:");
    return sel;
}

inline SEL sel_set_title_visibility() {
    static auto sel = sel_registerName("setTitleVisibility:");
    return sel;
}

inline SEL sel_set_titlebar_appears_transparent() {
    static auto sel = sel_registerName("setTitlebarAppearsTransparent:");
    return sel;
}

inline SEL sel_set_background_color() {
    static auto sel = sel_registerName("setBackgroundColor:");
    return sel;
}

inline SEL sel_background_color() {
    static auto sel = sel_registerName("backgroundColor");
    return sel;
}

inline SEL sel_clear_color() {
    static auto sel = sel_registerName("clearColor");
    return sel;
}

inline SEL sel_set_opaque() {
    static auto sel = sel_registerName("setOpaque:");
    return sel;
}

inline SEL sel_is_opaque() {
    static auto sel = sel_registerName("isOpaque");
    return sel;
}

inline SEL sel_titlebar_appears_transparent() {
    static auto sel = sel_registerName("titlebarAppearsTransparent");
    return sel;
}

inline SEL sel_title_visibility() {
    static auto sel = sel_registerName("titleVisibility");
    return sel;
}

inline SEL sel_set_movable_by_window_background() {
    static auto sel = sel_registerName("setMovableByWindowBackground:");
    return sel;
}

inline SEL sel_is_movable_by_window_background() {
    static auto sel = sel_registerName("isMovableByWindowBackground");
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

// Used by the canvas gesture pipeline (Stage 8). `magnification`
// returns NSEvent's CGFloat magnify delta; `locationInWindow` lets
// us translate the gesture's focal point into content-view pixels.
inline SEL sel_magnification() {
    static auto sel = sel_registerName("magnification");
    return sel;
}

inline SEL sel_location_in_window() {
    static auto sel = sel_registerName("locationInWindow");
    return sel;
}

inline SEL sel_convert_point_from_view() {
    static auto sel = sel_registerName("convertPoint:fromView:");
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

inline SEL sel_preferred_scroller_style() {
    static auto sel = sel_registerName("preferredScrollerStyle");
    return sel;
}

inline SEL sel_preferred_languages() {
    static auto sel = sel_registerName("preferredLanguages");
    return sel;
}

inline SEL sel_count() {
    static auto sel = sel_registerName("count");
    return sel;
}

inline SEL sel_object_at_index() {
    static auto sel = sel_registerName("objectAtIndex:");
    return sel;
}

inline SEL sel_scroller_width_for_control_size_scroller_style() {
    static auto sel = sel_registerName("scrollerWidthForControlSize:scrollerStyle:");
    return sel;
}

inline SEL sel_system_font_size() {
    static auto sel = sel_registerName("systemFontSize");
    return sel;
}

inline SEL sel_small_system_font_size() {
    static auto sel = sel_registerName("smallSystemFontSize");
    return sel;
}

inline SEL sel_double_click_interval() {
    static auto sel = sel_registerName("doubleClickInterval");
    return sel;
}

inline SEL sel_key_repeat_delay() {
    static auto sel = sel_registerName("keyRepeatDelay");
    return sel;
}

inline SEL sel_key_repeat_interval() {
    static auto sel = sel_registerName("keyRepeatInterval");
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

inline SEL sel_shared_workspace() {
    static auto sel = sel_registerName("sharedWorkspace");
    return sel;
}

inline SEL sel_accessibility_display_should_reduce_transparency() {
    static auto sel = sel_registerName("accessibilityDisplayShouldReduceTransparency");
    return sel;
}

inline SEL sel_accessibility_display_should_increase_contrast() {
    static auto sel = sel_registerName("accessibilityDisplayShouldIncreaseContrast");
    return sel;
}

inline SEL sel_accessibility_display_should_reduce_motion() {
    static auto sel = sel_registerName("accessibilityDisplayShouldReduceMotion");
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

inline SEL sel_retain() {
    static auto sel = sel_registerName("retain");
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

inline SEL sel_scrolling_delta_x() {
    static auto sel = sel_registerName("scrollingDeltaX");
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

inline SEL sel_window_number() {
    static auto sel = sel_registerName("windowNumber");
    return sel;
}

inline SEL sel_shared_application() {
    static auto sel = sel_registerName("sharedApplication");
    return sel;
}

inline SEL sel_effective_appearance() {
    static auto sel = sel_registerName("effectiveAppearance");
    return sel;
}

inline SEL sel_current_appearance() {
    static auto sel = sel_registerName("currentAppearance");
    return sel;
}

inline SEL sel_name() {
    static auto sel = sel_registerName("name");
    return sel;
}

inline SEL sel_is_active() {
    static auto sel = sel_registerName("isActive");
    return sel;
}

inline SEL sel_is_visible() {
    static auto sel = sel_registerName("isVisible");
    return sel;
}

inline SEL sel_is_key_window() {
    static auto sel = sel_registerName("isKeyWindow");
    return sel;
}

inline SEL sel_is_main_window() {
    static auto sel = sel_registerName("isMainWindow");
    return sel;
}

inline SEL sel_is_hidden() {
    static auto sel = sel_registerName("isHidden");
    return sel;
}

inline SEL sel_standard_window_button() {
    static auto sel = sel_registerName("standardWindowButton:");
    return sel;
}

inline SEL sel_superview() {
    static auto sel = sel_registerName("superview");
    return sel;
}

inline SEL sel_collection_behavior() {
    static auto sel = sel_registerName("collectionBehavior");
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

inline float text_measure(float font_size, FontCacheKey const& key,
                          char const* text_ptr, unsigned int len);

inline float text_measure(float font_size, bool mono,
                          char const* text_ptr, unsigned int len) {
    FontCacheKey key = default_text_font_key(mono);
    return text_measure(font_size, key, text_ptr, len);
}

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

inline std::string cf_string_to_utf8(CFStringRef value) {
    if (!value)
        return {};
    CFIndex const chars = CFStringGetLength(value);
    CFIndex const max_bytes = CFStringGetMaximumSizeForEncoding(
        chars, kCFStringEncodingUTF8);
    std::string out;
    out.resize(static_cast<std::size_t>(max_bytes + 1), '\0');
    if (!CFStringGetCString(value, out.data(), max_bytes + 1,
                            kCFStringEncodingUTF8)) {
        return {};
    }
    out.resize(std::strlen(out.c_str()));
    return out;
}

inline std::string ns_string_to_utf8(id value) {
    if (!value || !objc_responds_to(value, sel_utf8_string()))
        return {};
    char const* raw = objc_send<char const*>(value, sel_utf8_string());
    return raw && raw[0] != '\0' ? std::string{raw} : std::string{};
}

inline std::string macos_preferred_locale() {
    auto locale_class = static_cast<Class>(objc_getClass("NSLocale"));
    if (!locale_class)
        return {};
    auto locale_type = class_as_id(locale_class);
    if (!objc_responds_to(locale_type, sel_preferred_languages()))
        return {};
    id languages = objc_send<id>(locale_type, sel_preferred_languages());
    if (!languages
        || !objc_responds_to(languages, sel_count())
        || !objc_responds_to(languages, sel_object_at_index())) {
        return {};
    }
    auto const count = objc_send<unsigned long>(languages, sel_count());
    if (count == 0)
        return {};
    id first = objc_send<id>(
        languages,
        sel_object_at_index(),
        static_cast<unsigned long>(0));
    return ns_string_to_utf8(first);
}

inline CTFontRef create_preferred_sans_font() {
    char const preferred[] = "Pretendard";
    auto cf_name = CFGuard<CFStringRef>(CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<UInt8 const*>(preferred),
        static_cast<CFIndex>(sizeof(preferred) - 1),
        kCFStringEncodingUTF8,
        false));
    if (!cf_name)
        return nullptr;

    CTFontRef font = CTFontCreateWithName(cf_name, 16.0, nullptr);
    if (!font)
        return nullptr;

    auto family = CFGuard<CFStringRef>(CTFontCopyFamilyName(font));
    auto family_name = cf_string_to_utf8(family.ref);
    if (family_name.find(preferred) != std::string::npos)
        return font;

    CFRelease(font);
    return nullptr;
}

inline void text_init() {
    if (g_text.initialized) return;
    g_text.sans = create_preferred_sans_font();
    if (!g_text.sans)
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
    for (auto& [_, ref] : g_text.cache) {
        if (ref) CFRelease(ref);
    }
    g_text.cache.clear();
    g_text.missing_logged.clear();
    g_text.initialized = false;
}

// Resolve a base 16pt CTFontRef for `key`. Empty family resolves to
// the system sans / monospace default; named family is resolved via
// CTFontCreateWithName, then weight + italic traits are applied via
// CTFontCreateCopyWithSymbolicTraits. Failures fall back to the
// system default with a one-shot stderr log keyed on family name.
//
// The returned reference is owned by `g_text.cache`; callers must NOT
// CFRelease it. Pass to CTFontCreateCopyWithAttributes() to obtain a
// per-call sized copy (which the caller does own).
inline CTFontRef acquire_base_font(FontCacheKey const& key) {
    if (!g_text.initialized) text_init();
    auto it = g_text.cache.find(key);
    if (it != g_text.cache.end()) return it->second;

    auto fallback = [&](char const* reason) -> CTFontRef {
        if (!key.family.empty()
            && g_text.missing_logged.insert(key.family).second) {
            std::fprintf(stderr,
                "[text] font '%s' (%s) not resolved (%s); using system default\n",
                key.family.c_str(),
                key.weight == ::phenotype::FontWeight::Bold ? "Bold" : "Regular",
                reason);
        }
        return key.mono ? g_text.mono : g_text.sans;
    };

    CTFontRef base = nullptr;
    if (key.family.empty()) {
        base = key.mono ? g_text.mono : g_text.sans;
        if (base) base = static_cast<CTFontRef>(CFRetain(base));
    } else {
        // Look up via process-registered alias first — a caller that
        // ran `register_font_file_macos("MyAlias", "/path/to/face.ttf")`
        // expects FontSpec{family="MyAlias"} to resolve to the
        // registered face's actual PostScript name (which is what
        // CTFontCreateWithName actually accepts after the registration
        // step).
        std::string lookup_name = key.family;
        if (auto it = g_text.registered_aliases.find(key.family);
            it != g_text.registered_aliases.end()) {
            lookup_name = it->second;
        } else if (!key.mono && key.family == "Pretendard") {
            base = g_text.sans
                ? static_cast<CTFontRef>(CFRetain(g_text.sans))
                : nullptr;
        }
        if (!base) {
            auto cf_name = CFGuard<CFStringRef>(CFStringCreateWithBytes(
                kCFAllocatorDefault,
                reinterpret_cast<UInt8 const*>(lookup_name.data()),
                static_cast<CFIndex>(lookup_name.size()),
                kCFStringEncodingUTF8,
                false));
            if (!cf_name) {
                CTFontRef fb = fallback("CFString create failed");
                if (fb) g_text.cache.emplace(key, static_cast<CTFontRef>(CFRetain(fb)));
                return fb;
            }
            base = CTFontCreateWithName(cf_name, 16.0, nullptr);
        }
    }
    if (!base) {
        CTFontRef fb = fallback("CTFontCreateWithName returned null");
        if (fb) g_text.cache.emplace(key, static_cast<CTFontRef>(CFRetain(fb)));
        return fb;
    }

    // Apply Bold / Italic via symbolic traits.
    CTFontSymbolicTraits desired = 0;
    if (key.weight == ::phenotype::FontWeight::Bold)   desired |= kCTFontTraitBold;
    if (key.style  == ::phenotype::FontStyle::Italic)  desired |= kCTFontTraitItalic;
    if (desired != 0) {
        CTFontRef styled = CTFontCreateCopyWithSymbolicTraits(
            base, 16.0, nullptr, desired,
            kCTFontTraitBold | kCTFontTraitItalic);
        if (styled) {
            CFRelease(base);
            base = styled;
        }
    }
    g_text.cache.emplace(key, base);
    return base;
}

// platform_api::text.register_font_file entry. Registers `path` with
// CoreText (process-scope) and stashes `family_alias → resolved
// PostScript name` in the alias map so subsequent FontSpec lookups by
// `family_alias` route to the registered face. Returns false on any
// failure (file missing, unsupported format, registration rejected) so
// the caller can fall back to its default-font path; on success any
// previously cached resolution for the same alias is invalidated so
// the next render picks up the newly registered face.
inline bool text_register_font_file(char const* family_alias,
                                    unsigned int alias_len,
                                    char const* path,
                                    unsigned int path_len) {
    if (!g_text.initialized) text_init();
    if (family_alias == nullptr || alias_len == 0
        || path == nullptr || path_len == 0) return false;

    auto cf_path = CFGuard<CFStringRef>(CFStringCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<UInt8 const*>(path),
        static_cast<CFIndex>(path_len),
        kCFStringEncodingUTF8,
        false));
    if (!cf_path) return false;
    auto cf_url = CFGuard<CFURLRef>(CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault, cf_path, kCFURLPOSIXPathStyle, false));
    if (!cf_url) return false;

    CFErrorRef err = nullptr;
    Boolean const ok = CTFontManagerRegisterFontsForURL(
        cf_url, kCTFontManagerScopeProcess, &err);
    if (!ok) {
        // Already-registered (kCTFontManagerErrorAlreadyRegistered = 105)
        // is fine — the file can ship in multiple STYLE entries; treat
        // it as a successful no-op so we still record the alias below.
        bool already = false;
        if (err != nullptr) {
            if (CFErrorGetCode(err) == 105) already = true;
            CFRelease(err);
        }
        if (!already) return false;
    }

    // Read the resolved PostScript name from the registered face — that
    // is the string CTFontCreateWithName actually accepts post-register.
    CFArrayRef descs = CTFontManagerCreateFontDescriptorsFromURL(cf_url);
    if (descs == nullptr || CFArrayGetCount(descs) == 0) {
        if (descs) CFRelease(descs);
        return false;
    }
    CTFontDescriptorRef desc = static_cast<CTFontDescriptorRef>(
        CFArrayGetValueAtIndex(descs, 0));
    auto cf_psname = CFGuard<CFStringRef>(static_cast<CFStringRef>(
        CTFontDescriptorCopyAttribute(desc, kCTFontNameAttribute)));
    CFRelease(descs);
    if (!cf_psname) return false;

    // CFStringRef → std::string (UTF-8).
    CFIndex const ps_chars = CFStringGetLength(cf_psname);
    CFIndex const ps_max = CFStringGetMaximumSizeForEncoding(
        ps_chars, kCFStringEncodingUTF8);
    std::string ps_name;
    ps_name.resize(static_cast<std::size_t>(ps_max + 1), '\0');
    if (!CFStringGetCString(cf_psname, ps_name.data(),
                            ps_max + 1, kCFStringEncodingUTF8)) {
        return false;
    }
    ps_name.resize(std::strlen(ps_name.c_str()));

    std::string const alias{family_alias, alias_len};
    auto [it, inserted] = g_text.registered_aliases.insert_or_assign(
        alias, std::move(ps_name));
    (void)it; (void)inserted;

    // Drop any cached base-font entries that match this alias so the
    // next acquire_base_font re-resolves through the new mapping.
    for (auto cit = g_text.cache.begin(); cit != g_text.cache.end(); ) {
        if (cit->first.family == alias) {
            if (cit->second) CFRelease(cit->second);
            cit = g_text.cache.erase(cit);
        } else {
            ++cit;
        }
    }
    g_text.missing_logged.erase(alias);
    return true;
}

struct TextLineMetrics {
    float logical_width = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    float leading = 0.0f;
    CGRect glyph_bounds = CGRectNull;
};

inline CFGuard<CTFontRef> copy_text_font(float font_size, FontCacheKey const& key,
                                         float width_factor = 1.0f) {
    CTFontRef base = acquire_base_font(key);
    if (!base) return CFGuard<CTFontRef>(nullptr);
    // Apply the FontSpec width-factor as a font matrix: scaling X by
    // `width_factor` and leaving Y at 1.0 stretches each glyph along
    // the run's horizontal axis without changing baseline metrics or
    // line height. CTFontCreateCopyWithAttributes accepts a matrix
    // pointer; passing the identity (factor == 1.0) is the same as
    // passing nullptr and round-trips the pre-stretch behaviour
    // bit-for-bit.
    if (width_factor == 1.0f) {
        return CFGuard<CTFontRef>(
            CTFontCreateCopyWithAttributes(base, font_size, nullptr, nullptr));
    }
    CGAffineTransform const m = CGAffineTransformMakeScale(
        static_cast<CGFloat>(width_factor), CGFloat{1.0});
    return CFGuard<CTFontRef>(
        CTFontCreateCopyWithAttributes(base, font_size, &m, nullptr));
}

inline CFGuard<CTFontRef> copy_text_font(float font_size, bool mono) {
    FontCacheKey key = default_text_font_key(mono);
    return copy_text_font(font_size, key);
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

inline float text_measure(float font_size, FontCacheKey const& key,
                          char const* text_ptr, unsigned int len) {
    if (len == 0) return 0.0f;
    if (!g_text.initialized) text_init();
    if (!g_text.initialized) return 0.0f;

    auto font = copy_text_font(font_size, key);
    if (!font)
        return 0.0f;

    auto line = create_text_line(font, text_ptr, len);
    if (!line)
        return 0.0f;

    double width = CTLineGetTypographicBounds(line, nullptr, nullptr, nullptr);
    return std::isfinite(width) ? static_cast<float>(width) : 0.0f;
}

// platform_api::text.measure entry point. Constructs the FontCacheKey
// from the wire-format flags + family bytes and delegates.
inline float text_measure_api(float font_size, unsigned int flags,
                              char const* font_family, unsigned int family_len,
                              char const* text, unsigned int len) {
    FontCacheKey key{};
    if (font_family && family_len > 0)
        key.family.assign(font_family, family_len);
    key.weight = (flags & 2u) ? ::phenotype::FontWeight::Bold   : ::phenotype::FontWeight::Regular;
    key.style  = (flags & 4u) ? ::phenotype::FontStyle::Italic  : ::phenotype::FontStyle::Upright;
    key.mono   = (flags & 1u) != 0;
    return text_measure(font_size, key, text, len);
}

// Pull ascent / descent / leading / cap_height for the resolved face
// at `font_size`. CoreText reports all four on `CTFontRef` directly
// (already scaled to the font's point size), so there's no need to
// build a CTLine. `width_factor` doesn't affect vertical metrics — the
// font matrix only scales X — so we always measure with the natural
// matrix.
inline bool text_metrics(float font_size, FontCacheKey const& key,
                         float& out_ascent, float& out_descent,
                         float& out_leading, float& out_cap_height) {
    out_ascent = 0.0f;
    out_descent = 0.0f;
    out_leading = 0.0f;
    out_cap_height = 0.0f;
    if (!g_text.initialized) text_init();
    if (!g_text.initialized) return false;
    auto font = copy_text_font(font_size, key);
    if (!font) return false;
    out_ascent     = static_cast<float>(CTFontGetAscent(font.ref));
    out_descent    = static_cast<float>(CTFontGetDescent(font.ref));
    out_leading    = static_cast<float>(CTFontGetLeading(font.ref));
    out_cap_height = static_cast<float>(CTFontGetCapHeight(font.ref));
    return true;
}

// platform_api::text.metrics entry point. Mirrors `text_measure_api`'s
// flag / family decoding.
inline void text_metrics_api(float font_size, unsigned int flags,
                             char const* font_family, unsigned int family_len,
                             float* out_ascent, float* out_descent,
                             float* out_leading,
                             float* out_cap_height) {
    if (out_ascent)     *out_ascent     = 0.0f;
    if (out_descent)    *out_descent    = 0.0f;
    if (out_leading)    *out_leading    = 0.0f;
    if (out_cap_height) *out_cap_height = 0.0f;
    FontCacheKey key{};
    if (font_family && family_len > 0)
        key.family.assign(font_family, family_len);
    key.weight = (flags & 2u) ? ::phenotype::FontWeight::Bold   : ::phenotype::FontWeight::Regular;
    key.style  = (flags & 4u) ? ::phenotype::FontStyle::Italic  : ::phenotype::FontStyle::Upright;
    key.mono   = (flags & 1u) != 0;
    float a = 0.0f, d = 0.0f, l = 0.0f, c = 0.0f;
    if (!text_metrics(font_size, key, a, d, l, c)) return;
    if (out_ascent)     *out_ascent     = a;
    if (out_descent)    *out_descent    = d;
    if (out_leading)    *out_leading    = l;
    if (out_cap_height) *out_cap_height = c;
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

        FontCacheKey key{};
        key.family = entry.family;
        key.weight = entry.weight;
        key.style  = entry.style;
        key.mono   = entry.mono;
        auto font = copy_text_font(entry.font_size, key);
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

struct MaterialInstanceGPU {
    float rect[4]{};
    float tint[4]{};
    // radius, blur radius, opacity, resolved sample taps
    float params[4]{};
    // saturation, luminance floor, luminance gain, edge highlight
    float optics[4]{};
    // edge width, shadow alpha, shadow radius, noise opacity
    float effects[4]{};
    // blur step scale, kernel radius, reserved, reserved
    float sampling[4]{};
    // gamma, midpoint, contrast, edge lift
    float luminance_curve[4]{};
    // specular anchor x/y, radius, intensity
    float interaction[4]{};
    // pointer lens anchor x/y, radius, strength
    float interaction_lens[4]{};
    // control morph scale delta, depth, edge lift, shadow compression
    float control_morph[4]{};
    // materialize wave strength, edge lift, lensing gain, rim position
    float transition_optics[4]{};
    // refraction strength, edge bias, max offset pixels, edge caustic intensity
    float refraction[4]{};
    // bevel width, inner highlight, outer shadow, chromatic fringe
    float edge_optics[4]{};
    // spectral warmth, coolness, dispersion, rim tint
    float spectral_tint[4]{};
    // dynamic light direction x/y, highlight strength, shadow strength
    float lighting[4]{};
    // glass thickness, lensing gain, shadow gain, scattering gain
    float thickness[4]{};
    // axial offset, tangential offset, prismatic gain, caustic spread
    float dispersion[4]{};
    // fade extent, dissolve, dimming, hard style
    float scroll_edge[4]{};
    // intensity, tint weight, edge lift, lensing gain
    float prominent_glass[4]{};
    // dimming, contrast lift, brightness response, detail response
    float clear_glass[4]{};
    // group x/y/w/h for container edge-continuity execution
    float group_rect[4]{};
    // shape blend strength, inner-edge alpha blend strength, flags, command index
    float group_effects[4]{};
    // fusion strength, lensing gain, edge lift, shadow gain
    float fusion_optics[4]{};
    // bridge direction x/y, anchor x/y for container/matched flow
    float bridge_motion[4]{};
    // bridge strength, flow offset gain, ribbon width, highlight gain
    float bridge_optics[4]{};
};

// Per-vertex GPU layout for the triangle pipeline (FillPath fast path).
// Each ear-clipped triangle pushes 3 of these into the batch's triangle
// vertex buffer; the GPU rasterises with hardware coverage rules so
// adjacent triangles tile pixel-perfectly without sub-pixel gaps. 24
// bytes/vertex × 3 vertices/triangle = 72 bytes/triangle, vs. ~50
// scanlines × 48 bytes/ColorInstance = ~2.4 KB/triangle under the old
// CPU scanline path. A 36k-hatch DWG (colorwh.dwg) drops from millions
// of fill_rect instances to ~70k vertices in one drawPrimitives call.
struct TriVertexGPU {
    float pos[2]{};
    float color[4]{};
};

// Per-arc instance for the SDF arc pipeline. Layout matches the
// shader's `ArcInstance` struct exactly (3 × float4 = 48 bytes).
//   center_radius_thickness = (cx, cy, radius, thickness)
//   angles                  = (start_angle, end_angle, _, _)
//   color                   = (r, g, b, a) linear, 0..1
struct ArcInstanceGPU {
    float center_radius_thickness[4]{};
    float angles[4]{};
    float color[4]{};
};

struct TextInstanceGPU {
    float rect[4]{};
    float uv_rect[4]{};
    float color[4]{};
    // Per-instance rigid-body rotation: `(pivot_x, pivot_y, cos, sin)`.
    // The shader rotates each glyph quad's pre-rotation pixel position
    // around `(pivot_x, pivot_y)` by the angle whose cosine and sine
    // are stored here. Default `(0,0,1,0)` is identity — packs the
    // axis-aligned glyph instance bit-for-bit the way it always did.
    float rot[4]{0.0f, 0.0f, 1.0f, 0.0f};
};

struct ParsedTextRun {
    float x = 0.0f;
    float y = 0.0f;
    float font_size = 0.0f;
    // Radians, CCW about pivot `(x, y)`. Default 0.0f reproduces
    // the unrotated glyph atlas path; nonzero passes through to the
    // text vertex shader via TextInstanceGPU::rot.
    float rotation = 0.0f;
    // Horizontal glyph stretch — see `FontSpec::width_factor`. We
    // multiply each glyph's pixel-space horizontal advance and width
    // by this when prepare_text_instances lays the run out, so the
    // glyphs end up rendered wider/narrower in place along the run's
    // local X axis. Default 1.0f keeps the atlas-native advance.
    float width_factor = 1.0f;
    bool mono = false;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
    float line_height = 0.0f;
    char const* text = nullptr;
    unsigned int len = 0;
    // Index into FrameScratch::batches that owns this run, or UINT32_MAX
    // for overlay runs (IME composition / generic caret) that always
    // render full-viewport above scissored scene content.
    std::uint32_t batch_idx = 0;
    // Resolved FontCacheKey (family / weight / style / mono); driven
    // by the wire-format flags + family bytes at decode time.
    FontCacheKey font_key{};
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
    int font_size_key = 0;
    int line_height_key = 0;
    int scale_key = 0;
    // Quantised FontSpec.width_factor — different stretches need
    // different rasterised glyphs (the matrix bakes the X scale into
    // the atlas alpha pixels), so a single (text + font + size)
    // tuple can occupy multiple atlas entries when the same caller
    // draws the same string at different stretches.
    int width_factor_key = 0;
    bool mono = false;
    float x_offset = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    float uw = 0.0f;
    float vh = 0.0f;
    bool has_ink = false;
    FontCacheKey font_key{};
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
    // Index into FrameScratch::batches that owns this image. The lazy
    // image-cache resolution in prepare_image_instances pushes the
    // resolved instance (or placeholder colors) into the matching
    // batch's per-pipeline vector so scissor clipping survives the
    // decode → resolve split.
    std::uint32_t batch_idx = 0;
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

// Cmd::Scissor splits each frame into ordered batches. Within a batch
// instances are pushed in decode order; render time issues one
// vkCmdSetScissor / setScissorRect: per batch and per-pipeline draws
// using firstInstance / baseInstance offsets from the flat staging
// vectors that finalize_batches assembles.
//
// Sentinel rect (x=y=w=h=0.0) means "full drawable" — paint emits this
// to reset clipping (emit_scissor_reset). The first batch of every
// frame is initialized to this sentinel so any draw before a Scissor
// command renders unclipped.
struct ScissorBatch {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    // tri_vertices holds raw triangle-list vertices (3 per triangle)
    // accumulated from FillPath / FillQuads output. Drawn before colors
    // in the same batch; the decoder opens a same-scissor batch when a
    // later triangle primitive must appear above prior color instances.
    std::vector<TriVertexGPU>     tri_vertices;
    std::vector<ColorInstanceGPU> colors;
    std::vector<MaterialInstanceGPU> materials;
    std::vector<ArcInstanceGPU>   arcs;
    std::vector<ImageInstanceGPU> images;
    std::vector<TextInstanceGPU>  texts;
    // DrawText commands are decoded into text_runs first and materialized into
    // texts after atlas preparation. Track them here so text-only scissored
    // canvases still keep their own batch instead of being treated as empty.
    bool pending_text_runs = false;
    std::uint32_t tri_first   = 0;
    std::uint32_t color_first = 0;
    std::uint32_t material_first = 0;
    std::uint32_t arc_first   = 0;
    std::uint32_t image_first = 0;
    std::uint32_t text_first  = 0;
};

struct MaterialPaintLayerRange {
    std::uint32_t command_index = 0;
    std::size_t batch_index = 0;
    std::size_t first = 0;
    std::size_t count = 0;
};

struct FrameScratch {
    // Per-batch local accumulators. Concatenated into the flat
    // *_instances vectors below by finalize_batches().
    std::vector<ScissorBatch> batches;

    // Flat staging vectors uploaded to GPU instance buffers. Populated
    // by finalize_batches() — direct decode/prepare pushes go to the
    // per-batch locals above.
    std::vector<TriVertexGPU>     tri_vertices;
    std::vector<ColorInstanceGPU> color_instances;
    std::vector<MaterialInstanceGPU> material_instances;
    std::vector<ArcInstanceGPU>   arc_instances;
    std::vector<ImageInstanceGPU> image_instances;
    std::vector<TextInstanceGPU> text_instances;

    // Overlays (IME caret/underline + composition text) are Z-above
    // scene content and always render full-viewport, outside the
    // batch loop.
    std::vector<ColorInstanceGPU> overlay_color_instances;
    std::vector<TextInstanceGPU> overlay_text_instances;

    std::vector<ParsedTextRun> text_runs;
    std::vector<PendingImageCmd> images;
    std::vector<std::string> overlay_text_storage;
    std::vector<MaterialRuntimeRecord> material_records;
    std::vector<MaterialContainerExecutionDescriptor>
        material_container_execution_descriptors;
    std::vector<MaterialPaintLayerRange> material_paint_layer_ranges;
    std::uint32_t foreground_text_candidate_count = 0;
    std::uint32_t foreground_text_remap_count = 0;
    std::uint32_t full_frame_opaque_fill_count = 0;

    // Per-FillPath scratch buffers, reused across every Cmd::FillPath
    // decode in a single frame. CAD HATCH-heavy content (e.g.
    // colorwh.dwg with 36 095 fills) used to default-construct these
    // three std::vectors inside the case block, costing ~100k heap
    // allocations per frame. Hoisting them out keeps allocator
    // pressure flat as the canvas count scales.
    std::vector<float>       fill_polygon;
    std::vector<float>       fill_tris;
    std::vector<std::size_t> fill_ear_remain;

    // Slice within `text_instances` that contains overlay text. Set by
    // finalize_batches() — overlay texts are appended after all batched
    // scene texts so a single instance buffer holds both. Drawn after
    // the batch loop with full-viewport scissor.
    std::uint32_t overlay_text_first = 0;
    std::uint32_t overlay_text_count = 0;

    void clear() {
        batches.clear();
        batches.push_back(ScissorBatch{});  // sentinel: full viewport
        tri_vertices.clear();
        color_instances.clear();
        material_instances.clear();
        arc_instances.clear();
        overlay_color_instances.clear();
        text_runs.clear();
        images.clear();
        image_instances.clear();
        text_instances.clear();
        overlay_text_instances.clear();
        overlay_text_storage.clear();
        material_records.clear();
        material_container_execution_descriptors.clear();
        material_paint_layer_ranges.clear();
        foreground_text_candidate_count = 0;
        foreground_text_remap_count = 0;
        full_frame_opaque_fill_count = 0;
        overlay_text_first = 0;
        overlay_text_count = 0;
    }
};

inline void open_scissor_batch(FrameScratch& s,
                               float x, float y, float w, float h) {
    auto& cur = s.batches.back();
    if (cur.tri_vertices.empty() && cur.colors.empty()
        && cur.materials.empty() && cur.arcs.empty()
        && cur.images.empty() && cur.texts.empty()
        && !cur.pending_text_runs) {
        cur.x = x; cur.y = y; cur.w = w; cur.h = h;
        return;
    }
    ScissorBatch next;
    next.x = x; next.y = y; next.w = w; next.h = h;
    s.batches.push_back(std::move(next));
}

inline void open_same_scissor_batch(FrameScratch& s) {
    auto const& cur = s.batches.back();
    open_scissor_batch(s, cur.x, cur.y, cur.w, cur.h);
}

inline void ensure_triangle_order_batch(FrameScratch& s) {
    auto const& cur = s.batches.back();
    if (!cur.colors.empty() || !cur.materials.empty() || !cur.arcs.empty()
        || !cur.images.empty() || !cur.texts.empty()
        || cur.pending_text_runs) {
        open_same_scissor_batch(s);
    }
}

inline void finalize_batches(FrameScratch& s) {
    s.tri_vertices.clear();
    s.color_instances.clear();
    s.material_instances.clear();
    s.arc_instances.clear();
    s.image_instances.clear();
    s.text_instances.clear();
    for (auto& b : s.batches) {
        b.tri_first   = static_cast<std::uint32_t>(s.tri_vertices.size());
        b.color_first = static_cast<std::uint32_t>(s.color_instances.size());
        b.material_first = static_cast<std::uint32_t>(s.material_instances.size());
        b.arc_first   = static_cast<std::uint32_t>(s.arc_instances.size());
        b.image_first = static_cast<std::uint32_t>(s.image_instances.size());
        b.text_first  = static_cast<std::uint32_t>(s.text_instances.size());
        s.tri_vertices.insert(s.tri_vertices.end(),
                              b.tri_vertices.begin(), b.tri_vertices.end());
        s.color_instances.insert(s.color_instances.end(),
                                 b.colors.begin(), b.colors.end());
        s.material_instances.insert(s.material_instances.end(),
                                    b.materials.begin(), b.materials.end());
        s.arc_instances.insert(s.arc_instances.end(),
                               b.arcs.begin(), b.arcs.end());
        s.image_instances.insert(s.image_instances.end(),
                                 b.images.begin(), b.images.end());
        s.text_instances.insert(s.text_instances.end(),
                                b.texts.begin(), b.texts.end());
    }
    s.overlay_text_first =
        static_cast<std::uint32_t>(s.text_instances.size());
    s.overlay_text_count =
        static_cast<std::uint32_t>(s.overlay_text_instances.size());
    s.text_instances.insert(s.text_instances.end(),
                            s.overlay_text_instances.begin(),
                            s.overlay_text_instances.end());
}

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

inline void append_tri_vertex(std::vector<TriVertexGPU>& out,
                              float x, float y,
                              float r, float g, float b, float a) {
    TriVertexGPU v;
    v.pos[0] = x;
    v.pos[1] = y;
    v.color[0] = r;
    v.color[1] = g;
    v.color[2] = b;
    v.color[3] = a;
    out.push_back(v);
}

inline bool stroked_segment_needs_triangle_body(float x1,
                                                float y1,
                                                float x2,
                                                float y2,
                                                float thickness) noexcept {
    if (thickness <= 0.0f)
        return false;
    float const dx = x2 - x1;
    float const dy = y2 - y1;
    float const line_len = std::sqrt(dx * dx + dy * dy);
    if (line_len <= 0.0f)
        return false;
    return std::fabs(dx) > 0.0001f && std::fabs(dy) > 0.0001f;
}

inline void append_stroked_segment_to_batch(ScissorBatch& batch,
                                            float x1, float y1,
                                            float x2, float y2,
                                            float thickness,
                                            float r, float g,
                                            float b, float a) {
    float const dx = x2 - x1;
    float const dy = y2 - y1;
    float const line_len = std::sqrt(dx * dx + dy * dy);
    if (line_len <= 0.0f || thickness <= 0.0f)
        return;

    float const half_th = thickness * 0.5f;
    if (!stroked_segment_needs_triangle_body(x1, y1, x2, y2, thickness)) {
        float const w = (std::fabs(dy) <= 0.0001f) ? line_len : thickness;
        float const h = (std::fabs(dx) <= 0.0001f) ? line_len : thickness;
        float const x = (std::fabs(dx) <= 0.0001f)
            ? x1 - half_th
            : (x1 < x2 ? x1 : x2);
        float const y = (std::fabs(dy) <= 0.0001f)
            ? y1 - half_th
            : (y1 < y2 ? y1 : y2);
        append_color_instance(
            batch.colors,
            x, y, w, h, r, g, b, a,
            half_th, 0.0f, 2.0f);
        return;
    }

    float const nx = -dy / line_len * half_th;
    float const ny = dx / line_len * half_th;
    auto& tris = batch.tri_vertices;
    append_tri_vertex(tris, x1 + nx, y1 + ny, r, g, b, a);
    append_tri_vertex(tris, x2 + nx, y2 + ny, r, g, b, a);
    append_tri_vertex(tris, x2 - nx, y2 - ny, r, g, b, a);
    append_tri_vertex(tris, x1 + nx, y1 + ny, r, g, b, a);
    append_tri_vertex(tris, x2 - nx, y2 - ny, r, g, b, a);
    append_tri_vertex(tris, x1 - nx, y1 - ny, r, g, b, a);

    append_color_instance(
        batch.colors,
        x1 - half_th, y1 - half_th, thickness, thickness,
        r, g, b, a, half_th, 0.0f, 2.0f);
    append_color_instance(
        batch.colors,
        x2 - half_th, y2 - half_th, thickness, thickness,
        r, g, b, a, half_th, 0.0f, 2.0f);
}

inline void append_stroked_segment(FrameScratch& scratch,
                                   float x1, float y1,
                                   float x2, float y2,
                                   float thickness,
                                   float r, float g,
                                   float b, float a) {
    if (stroked_segment_needs_triangle_body(x1, y1, x2, y2, thickness))
        ensure_triangle_order_batch(scratch);
    append_stroked_segment_to_batch(
        scratch.batches.back(),
        x1, y1, x2, y2,
        thickness,
        r, g, b, a);
}

inline void append_linear_gradient_instances(
        std::vector<ColorInstanceGPU>& out,
        float x, float y, float w, float h,
        Color from, Color to, GradientAxis axis, unsigned int steps) {
    if (w == 0.0f || h == 0.0f)
        return;
    if (w < 0.0f) { x += w; w = -w; }
    if (h < 0.0f) { y += h; h = -h; }

    unsigned int const count = linear_gradient_step_count(steps);
    out.reserve(out.size() + count);
    for (unsigned int i = 0; i < count; ++i) {
        float const t = count == 1
            ? 0.0f
            : static_cast<float>(i) / static_cast<float>(count - 1);
        auto const color = lerp_color(from, to, t);
        if (axis == GradientAxis::Horizontal) {
            float const x0 = x + w * static_cast<float>(i)
                / static_cast<float>(count);
            float const x1 = x + w * static_cast<float>(i + 1)
                / static_cast<float>(count);
            append_color_instance(
                out,
                x0,
                y,
                x1 - x0,
                h,
                color.r / 255.0f,
                color.g / 255.0f,
                color.b / 255.0f,
                color.a / 255.0f);
        } else {
            float const y0 = y + h * static_cast<float>(i)
                / static_cast<float>(count);
            float const y1 = y + h * static_cast<float>(i + 1)
                / static_cast<float>(count);
            append_color_instance(
                out,
                x,
                y0,
                w,
                y1 - y0,
                color.r / 255.0f,
                color.g / 255.0f,
                color.b / 255.0f,
                color.a / 255.0f);
        }
    }
}

inline void append_material_instance(std::vector<MaterialInstanceGPU>& out,
                                     MaterialPlan const& plan,
                                     std::uint32_t command_index) {
    MaterialInstanceGPU inst{};
    auto const background_inflate = material_background_paint_inflate(plan);
    auto const background_soft_edge = plan.glass_background.feathered
        ? std::max(0.0f, plan.glass_background.soft_edge_radius)
        : 0.0f;
    auto const geometry = material_surface_execution_geometry(plan);
    if (!geometry.active)
        return;
    inst.rect[0] = geometry.x;
    inst.rect[1] = geometry.y;
    inst.rect[2] = geometry.w;
    inst.rect[3] = geometry.h;
    inst.tint[0] = plan.tint.r / 255.0f;
    inst.tint[1] = plan.tint.g / 255.0f;
    inst.tint[2] = plan.tint.b / 255.0f;
    inst.tint[3] = plan.tint.a / 255.0f;
    inst.params[0] = geometry.radius;
    inst.params[1] = plan.blur_radius;
    inst.params[2] = plan.opacity;
    inst.params[3] = static_cast<float>(plan.sample_taps);
    inst.optics[0] = plan.saturation;
    inst.optics[1] = plan.luminance_floor;
    inst.optics[2] = plan.luminance_gain;
    inst.optics[3] = plan.edge_highlight;
    inst.effects[0] = plan.edge_width;
    inst.effects[1] = plan.shadow_alpha;
    inst.effects[2] = plan.shadow_radius;
    inst.effects[3] = plan.noise_opacity;
    inst.sampling[0] = plan.sampling_kernel.blur_step_scale;
    inst.sampling[1] = static_cast<float>(plan.sampling_kernel.radius);
    inst.sampling[2] = background_soft_edge;
    inst.sampling[3] = background_inflate;
    inst.luminance_curve[0] = plan.luminance_curve.gamma;
    inst.luminance_curve[1] = plan.luminance_curve.midpoint;
    inst.luminance_curve[2] = plan.luminance_curve.contrast;
    inst.luminance_curve[3] = plan.luminance_curve.edge_lift;
    inst.interaction[0] = material_surface_execution_anchor_x(
        plan,
        geometry,
        plan.specular.anchor_x);
    inst.interaction[1] = material_surface_execution_anchor_y(
        plan,
        geometry,
        plan.specular.anchor_y);
    inst.interaction[2] = plan.specular.radius;
    inst.interaction[3] = plan.specular.intensity;
    inst.interaction_lens[0] = material_surface_execution_anchor_x(
        plan,
        geometry,
        plan.interaction.pointer_lens_anchor_x);
    inst.interaction_lens[1] = material_surface_execution_anchor_y(
        plan,
        geometry,
        plan.interaction.pointer_lens_anchor_y);
    inst.interaction_lens[2] = plan.interaction.pointer_lens_radius;
    inst.interaction_lens[3] = plan.interaction.pointer_lens_strength;
    inst.control_morph[0] = plan.interaction.control_morph_scale_delta;
    inst.control_morph[1] = plan.interaction.control_morph_depth;
    inst.control_morph[2] = plan.interaction.control_morph_edge;
    inst.control_morph[3] = plan.interaction.control_morph_shadow;
    inst.transition_optics[0] = plan.transition.materialize_wave_strength;
    inst.transition_optics[1] = plan.transition.materialize_edge_lift;
    inst.transition_optics[2] = plan.transition.materialize_lensing_gain;
    inst.transition_optics[3] = plan.transition.materialize_rim_position;
    inst.refraction[0] = plan.refraction.strength;
    inst.refraction[1] = plan.refraction.edge_bias;
    inst.refraction[2] = plan.refraction.max_offset_pixels;
    inst.refraction[3] = plan.refraction.edge_caustic_intensity;
    inst.edge_optics[0] = plan.edge_optics.bevel_width;
    inst.edge_optics[1] = plan.edge_optics.inner_highlight;
    inst.edge_optics[2] = plan.edge_optics.outer_shadow;
    inst.edge_optics[3] = plan.edge_optics.chromatic_fringe;
    inst.spectral_tint[0] = plan.spectral_tint.warmth;
    inst.spectral_tint[1] = plan.spectral_tint.coolness;
    inst.spectral_tint[2] = plan.spectral_tint.dispersion;
    inst.spectral_tint[3] = plan.spectral_tint.rim_tint;
    inst.lighting[0] = plan.dynamic_lighting.direction_x;
    inst.lighting[1] = plan.dynamic_lighting.direction_y;
    inst.lighting[2] = plan.dynamic_lighting.highlight_strength;
    inst.lighting[3] = plan.dynamic_lighting.shadow_strength;
    inst.thickness[0] = plan.glass_thickness.thickness;
    inst.thickness[1] = plan.glass_thickness.lensing_gain;
    inst.thickness[2] = plan.glass_thickness.shadow_gain;
    inst.thickness[3] = plan.glass_thickness.scattering_gain;
    inst.dispersion[0] = plan.glass_dispersion.axial_offset_pixels;
    inst.dispersion[1] = plan.glass_dispersion.tangential_offset_pixels;
    inst.dispersion[2] = plan.glass_dispersion.prismatic_gain;
    inst.dispersion[3] = plan.glass_dispersion.caustic_spread;
    inst.scroll_edge[0] = plan.scroll_edge.fade_extent_pixels;
    inst.scroll_edge[1] = plan.scroll_edge.dissolve_strength;
    inst.scroll_edge[2] = plan.scroll_edge.dimming_strength;
    inst.scroll_edge[3] = plan.scroll_edge.hard_style_strength;
    inst.prominent_glass[0] = plan.prominent_glass.intensity;
    inst.prominent_glass[1] = plan.prominent_glass.tint_weight;
    inst.prominent_glass[2] = plan.prominent_glass.edge_lift;
    inst.prominent_glass[3] = plan.prominent_glass.lensing_gain;
    inst.clear_glass[0] = plan.clear_glass_legibility.dimming_strength;
    inst.clear_glass[1] = plan.clear_glass_legibility.contrast_lift;
    inst.clear_glass[2] = plan.clear_glass_legibility.brightness_response;
    inst.clear_glass[3] = plan.clear_glass_legibility.detail_response;
    inst.group_rect[0] = inst.rect[0];
    inst.group_rect[1] = inst.rect[1];
    inst.group_rect[2] = inst.rect[2];
    inst.group_rect[3] = inst.rect[3];
    inst.group_effects[3] = static_cast<float>(command_index);
    out.push_back(inst);
}

inline void apply_material_paint_layer_execution_ranges(FrameScratch& scratch);

inline void apply_material_container_execution_descriptors(
        FrameScratch& scratch) {
    if (scratch.material_records.empty())
        return;
    scratch.material_container_execution_descriptors.clear();
    scratch.material_container_execution_descriptors.reserve(
        scratch.material_records.size());
    for (std::size_t index = 0; index < scratch.material_records.size(); ++index) {
        auto const& record = scratch.material_records[index];
        auto const& plan = record.plan;
        if (!plan.container.participates || plan.container.container_id == 0u)
            continue;
        auto const container_id = plan.container.container_id;
        auto seen = false;
        for (std::size_t prior = 0; prior < index; ++prior) {
            auto const& prior_plan = scratch.material_records[prior].plan;
            if (prior_plan.container.participates
                && prior_plan.container.container_id == container_id) {
                seen = true;
                break;
            }
        }
        if (seen)
            continue;
        auto const group = accumulate_material_container_group(
            scratch.material_records,
            container_id);
        for (auto const& candidate : scratch.material_records) {
            if (!material_plan_in_container(candidate.plan, container_id))
                continue;
            scratch.material_container_execution_descriptors.push_back(
                material_container_execution_descriptor_from_group(
                    candidate,
                    scratch.material_records,
                    group));
        }
    }
    apply_material_paint_layer_execution_ranges(scratch);
    for (auto& inst : scratch.material_instances) {
        auto const command_index =
            static_cast<std::uint32_t>(
                std::max(0.0f, std::round(inst.group_effects[3])));
        inst.group_effects[3] = 0.0f;
        auto const* execution =
            static_cast<MaterialContainerExecutionDescriptor const*>(nullptr);
        for (auto const& candidate :
             scratch.material_container_execution_descriptors) {
            if (candidate.command_index == command_index) {
                execution = &candidate;
                break;
            }
        }
        if (!execution)
            continue;
        auto const* record =
            static_cast<MaterialRuntimeRecord const*>(nullptr);
        for (auto const& candidate : scratch.material_records) {
            if (candidate.command_index == command_index) {
                record = &candidate;
                break;
            }
        }
        if (record && execution->glass_effect_materialize_execution) {
            auto const transition =
                material_execution_transition(
                    record->plan.transition,
                    execution);
            auto const gain_ratio = [](float target_gain,
                                       float current_gain) noexcept {
                if (current_gain <= 0.0001f)
                    return std::clamp(target_gain, 0.0f, 1.5f);
                return std::clamp(target_gain / current_gain, 0.0f, 1.5f);
            };
            auto const opacity_ratio = gain_ratio(
                execution->glass_effect_materialize_opacity_gain,
                record->plan.transition.opacity_gain);
            auto const optical_ratio = gain_ratio(
                execution->glass_effect_materialize_optical_gain,
                record->plan.transition.optical_gain);
            auto const shadow_ratio = gain_ratio(
                execution->glass_effect_materialize_shadow_gain,
                record->plan.transition.shadow_gain);
            auto const refraction_ratio = gain_ratio(
                execution->glass_effect_materialize_refraction_gain,
                record->plan.transition.refraction_gain);
            auto const geometry =
                material_surface_execution_geometry(record->plan, execution);
            if (!geometry.active) {
                inst.rect[2] = 0.0f;
                inst.rect[3] = 0.0f;
                inst.params[2] = 0.0f;
                continue;
            }
            inst.rect[0] = geometry.x;
            inst.rect[1] = geometry.y;
            inst.rect[2] = geometry.w;
            inst.rect[3] = geometry.h;
            inst.params[0] = geometry.radius;
            inst.params[2] = std::clamp(
                inst.params[2] * opacity_ratio,
                0.0f,
                1.0f);
            inst.tint[3] = std::clamp(inst.tint[3] * opacity_ratio,
                                      0.0f,
                                      1.0f);
            inst.params[1] = std::clamp(inst.params[1] * optical_ratio,
                                        0.0f,
                                        36.0f);
            inst.optics[0] =
                1.0f + (inst.optics[0] - 1.0f) * optical_ratio;
            inst.optics[3] = std::clamp(inst.optics[3] * optical_ratio,
                                        0.0f,
                                        1.0f);
            inst.effects[1] = std::clamp(inst.effects[1] * shadow_ratio,
                                         0.0f,
                                         0.4f);
            inst.effects[2] =
                std::max(0.0f, inst.effects[2] * shadow_ratio);
            inst.effects[3] = std::clamp(inst.effects[3] * optical_ratio,
                                         0.0f,
                                         0.05f);
            inst.refraction[0] = std::clamp(
                inst.refraction[0] * refraction_ratio,
                0.0f,
                0.35f);
            inst.refraction[2] = std::clamp(
                inst.refraction[2] * refraction_ratio,
                0.0f,
                8.0f);
            inst.refraction[3] = std::clamp(
                inst.refraction[3] * refraction_ratio,
                0.0f,
                0.35f);
            inst.edge_optics[1] = std::clamp(
                inst.edge_optics[1] * optical_ratio,
                0.0f,
                0.90f);
            inst.edge_optics[2] = std::clamp(
                inst.edge_optics[2] * shadow_ratio,
                0.0f,
                0.80f);
            inst.edge_optics[3] = std::clamp(
                inst.edge_optics[3] * refraction_ratio,
                0.0f,
                0.16f);
            inst.spectral_tint[0] = std::clamp(
                inst.spectral_tint[0] * optical_ratio,
                0.0f,
                0.22f);
            inst.spectral_tint[1] = std::clamp(
                inst.spectral_tint[1] * optical_ratio,
                0.0f,
                0.22f);
            inst.spectral_tint[2] = std::clamp(
                inst.spectral_tint[2] * optical_ratio,
                0.0f,
                0.22f);
            inst.spectral_tint[3] = std::clamp(
                inst.spectral_tint[3] * optical_ratio,
                0.0f,
                0.28f);
            inst.interaction[3] = std::clamp(
                inst.interaction[3] * optical_ratio,
                0.0f,
                1.0f);
            inst.lighting[2] = std::clamp(
                inst.lighting[2] * optical_ratio,
                0.0f,
                0.45f);
            inst.lighting[3] = std::clamp(
                inst.lighting[3] * optical_ratio,
                0.0f,
                0.36f);
            inst.group_rect[0] = inst.rect[0];
            inst.group_rect[1] = inst.rect[1];
            inst.group_rect[2] = inst.rect[2];
            inst.group_rect[3] = inst.rect[3];
            inst.transition_optics[0] =
                transition.materialize_wave_strength;
            inst.transition_optics[1] = transition.materialize_edge_lift;
            inst.transition_optics[2] =
                transition.materialize_lensing_gain;
            inst.transition_optics[3] =
                transition.materialize_rim_position;
        }
        if (execution->group_bounds_valid && execution->shape_blend_execution) {
            if (record
                && (execution->glass_effect_match_execution
                    || execution->union_execution
                    || execution->group_surface_execution)) {
                auto const geometry = material_surface_execution_geometry(
                    record->plan,
                    execution);
                if (!geometry.active) {
                    inst.rect[2] = 0.0f;
                    inst.rect[3] = 0.0f;
                    inst.params[2] = 0.0f;
                    continue;
                }
                inst.rect[0] = geometry.x;
                inst.rect[1] = geometry.y;
                inst.rect[2] = geometry.w;
                inst.rect[3] = geometry.h;
                inst.params[0] = geometry.radius;
                inst.group_rect[0] = inst.rect[0];
                inst.group_rect[1] = inst.rect[1];
                inst.group_rect[2] = inst.rect[2];
                inst.group_rect[3] = inst.rect[3];
                auto const match_motion =
                    material_glass_effect_match_motion_optics(*execution);
                auto const bridge_motion = match_motion.active
                    ? MaterialGlassEffectMotionOptics{}
                    : material_container_bridge_motion_optics(
                        *record,
                        scratch.material_records,
                        *execution);
                auto const& container_motion = match_motion.active
                    ? match_motion
                    : bridge_motion;
                if (container_motion.active) {
                    inst.bridge_motion[0] = container_motion.direction_x;
                    inst.bridge_motion[1] = container_motion.direction_y;
                    inst.bridge_motion[2] =
                        container_motion.specular_anchor_x;
                    inst.bridge_motion[3] =
                        container_motion.specular_anchor_y;
                    inst.bridge_optics[0] = container_motion.strength;
                    inst.bridge_optics[1] =
                        container_motion.flow_offset_gain;
                    inst.bridge_optics[2] = container_motion.ribbon_width;
                    inst.bridge_optics[3] =
                        container_motion.highlight_gain;
                }
                if (execution->group_interaction_source_valid) {
                    inst.interaction[0] =
                        execution->group_interaction_specular_anchor_x;
                    inst.interaction[1] =
                        execution->group_interaction_specular_anchor_y;
                    inst.interaction[2] = std::clamp(
                        std::max(
                            inst.interaction[2],
                            execution->group_interaction_specular_radius),
                        0.0f,
                        1.0f);
                    inst.interaction[3] = std::clamp(
                        std::max(
                            inst.interaction[3],
                            execution->group_interaction_specular_intensity),
                        0.0f,
                        1.0f);
                } else if (record->plan.specular.interaction_driven) {
                    inst.interaction[0] = material_surface_execution_anchor_x(
                        record->plan,
                        geometry,
                        record->plan.specular.anchor_x);
                    inst.interaction[1] = material_surface_execution_anchor_y(
                        record->plan,
                        geometry,
                        record->plan.specular.anchor_y);
                } else if (container_motion.active) {
                    inst.interaction[0] = container_motion.specular_anchor_x;
                    inst.interaction[1] = container_motion.specular_anchor_y;
                    inst.interaction[2] = std::clamp(
                        std::max(
                            inst.interaction[2],
                            record->plan.specular.radius
                                + 0.04f * container_motion.strength),
                        0.0f,
                        1.0f);
                    inst.interaction[3] = std::clamp(
                        inst.interaction[3]
                            * container_motion.specular_intensity_gain
                            + 0.06f * container_motion.strength,
                        0.0f,
                        1.0f);
                } else {
                    inst.interaction[0] = 0.5f;
                    inst.interaction[1] = 0.5f;
                }
                if (execution->group_interaction_pointer_lens_active) {
                    inst.interaction_lens[0] =
                        execution->group_interaction_pointer_lens_anchor_x;
                    inst.interaction_lens[1] =
                        execution->group_interaction_pointer_lens_anchor_y;
                    inst.interaction_lens[2] = std::clamp(
                        std::max(
                            inst.interaction_lens[2],
                            execution->group_interaction_pointer_lens_radius),
                        0.0f,
                        1.0f);
                    inst.interaction_lens[3] = std::clamp(
                        std::max(
                            inst.interaction_lens[3],
                            execution
                                ->group_interaction_pointer_lens_strength),
                        0.0f,
                        1.0f);
                } else if (record->plan.interaction.pointer_lens_active) {
                    inst.interaction_lens[0] =
                        material_surface_execution_anchor_x(
                            record->plan,
                            geometry,
                            record->plan.interaction.pointer_lens_anchor_x);
                    inst.interaction_lens[1] =
                        material_surface_execution_anchor_y(
                            record->plan,
                            geometry,
                            record->plan.interaction.pointer_lens_anchor_y);
                }
                if (execution->group_interaction_control_morph_active) {
                    inst.control_morph[0] =
                        execution
                            ->group_interaction_control_morph_scale_delta;
                    inst.control_morph[1] =
                        execution->group_interaction_control_morph_depth;
                    inst.control_morph[2] =
                        execution->group_interaction_control_morph_edge;
                    inst.control_morph[3] =
                        execution->group_interaction_control_morph_shadow;
                }
                if (execution->group_interaction_refraction_active) {
                    inst.refraction[0] = std::clamp(
                        std::max(
                            inst.refraction[0],
                            execution
                                ->group_interaction_refraction_strength),
                        0.0f,
                        0.35f);
                    inst.refraction[1] = std::clamp(
                        std::max(
                            inst.refraction[1],
                            execution
                                ->group_interaction_refraction_edge_bias),
                        0.0f,
                        1.0f);
                    inst.refraction[2] = std::clamp(
                        std::max(
                            inst.refraction[2],
                            execution
                                ->group_interaction_refraction_max_offset_pixels),
                        0.0f,
                        5.0f);
                    inst.refraction[3] = std::clamp(
                        std::max(
                            inst.refraction[3],
                            execution
                                ->group_interaction_refraction_edge_caustic_intensity),
                        0.0f,
                        0.35f);
                }
                if (execution->group_interaction_dynamic_lighting_active) {
                    inst.lighting[0] =
                        execution
                            ->group_interaction_dynamic_light_direction_x;
                    inst.lighting[1] =
                        execution
                            ->group_interaction_dynamic_light_direction_y;
                    inst.lighting[2] = std::clamp(
                        std::max(
                            inst.lighting[2],
                            execution
                                ->group_interaction_dynamic_light_highlight),
                        0.0f,
                        0.45f);
                    inst.lighting[3] = std::clamp(
                        std::max(
                            inst.lighting[3],
                            execution
                                ->group_interaction_dynamic_light_shadow),
                        0.0f,
                        0.36f);
                }
                if (execution->group_interaction_glass_thickness_active) {
                    inst.thickness[0] = std::clamp(
                        std::max(
                            inst.thickness[0],
                            execution->group_interaction_glass_thickness),
                        0.0f,
                        0.78f);
                    inst.thickness[1] = std::clamp(
                        std::max(
                            inst.thickness[1],
                            execution
                                ->group_interaction_glass_lensing_gain),
                        1.0f,
                        1.48f);
                    inst.thickness[2] = std::clamp(
                        std::max(
                            inst.thickness[2],
                            execution
                                ->group_interaction_glass_shadow_gain),
                        1.0f,
                        1.44f);
                    inst.thickness[3] = std::clamp(
                        std::max(
                            inst.thickness[3],
                            execution
                                ->group_interaction_glass_scattering_gain),
                        1.0f,
                        1.40f);
                }
                if (execution->group_interaction_glass_dispersion_active) {
                    inst.dispersion[0] = std::clamp(
                        std::max(
                            inst.dispersion[0],
                            execution
                                ->group_interaction_glass_dispersion_axial_offset),
                        0.0f,
                        3.20f);
                    inst.dispersion[1] = std::clamp(
                        std::max(
                            inst.dispersion[1],
                            execution
                                ->group_interaction_glass_dispersion_tangential_offset),
                        0.0f,
                        2.45f);
                    inst.dispersion[2] = std::clamp(
                        std::max(
                            inst.dispersion[2],
                            execution
                                ->group_interaction_glass_dispersion_prismatic_gain),
                        1.0f,
                        1.75f);
                    inst.dispersion[3] = std::clamp(
                        std::max(
                            inst.dispersion[3],
                            execution
                                ->group_interaction_glass_dispersion_caustic_spread),
                        0.0f,
                        0.40f);
                }
                if (execution->glass_effect_match_appearance_active) {
                    auto const blend = std::clamp(
                        execution->glass_effect_match_appearance_blend,
                        0.0f,
                        1.0f);
                    if (execution->glass_effect_match_appearance_tint_active) {
                        inst.tint[0] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_tint_r,
                                inst.tint[0],
                                blend),
                            0.0f,
                            1.0f);
                        inst.tint[1] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_tint_g,
                                inst.tint[1],
                                blend),
                            0.0f,
                            1.0f);
                        inst.tint[2] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_tint_b,
                                inst.tint[2],
                                blend),
                            0.0f,
                            1.0f);
                        inst.tint[3] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_tint_a,
                                inst.tint[3],
                                blend),
                            0.0f,
                            1.0f);
                    }
                    if (execution
                            ->glass_effect_match_appearance_spectral_tint_active) {
                        inst.spectral_tint[0] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_spectral_tint_warmth,
                                inst.spectral_tint[0],
                                blend),
                            0.0f,
                            0.22f);
                        inst.spectral_tint[1] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_spectral_tint_coolness,
                                inst.spectral_tint[1],
                                blend),
                            0.0f,
                            0.22f);
                        inst.spectral_tint[2] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_spectral_tint_dispersion,
                                inst.spectral_tint[2],
                                blend),
                            0.0f,
                            0.22f);
                        inst.spectral_tint[3] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_spectral_tint_rim,
                                inst.spectral_tint[3],
                                blend),
                            0.0f,
                            0.28f);
                    }
                    if (execution
                            ->glass_effect_match_appearance_prominent_glass_active) {
                        inst.prominent_glass[0] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_prominent_glass_intensity,
                                inst.prominent_glass[0],
                                blend),
                            0.0f,
                            1.0f);
                        inst.prominent_glass[1] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_prominent_glass_tint_weight,
                                inst.prominent_glass[1],
                                blend),
                            0.0f,
                            0.74f);
                        inst.prominent_glass[2] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_prominent_glass_edge_lift,
                                inst.prominent_glass[2],
                                blend),
                            0.0f,
                            0.30f);
                        inst.prominent_glass[3] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_prominent_glass_lensing_gain,
                                inst.prominent_glass[3],
                                blend),
                            1.0f,
                            1.36f);
                    }
                    if (execution
                            ->glass_effect_match_appearance_clear_glass_active) {
                        inst.clear_glass[0] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_clear_glass_dimming,
                                inst.clear_glass[0],
                                blend),
                            0.0f,
                            0.34f);
                        inst.clear_glass[1] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_clear_glass_contrast,
                                inst.clear_glass[1],
                                blend),
                            0.0f,
                            0.28f);
                        inst.clear_glass[2] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_clear_glass_brightness_response,
                                inst.clear_glass[2],
                                blend),
                            0.0f,
                            1.0f);
                        inst.clear_glass[3] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_clear_glass_detail_response,
                                inst.clear_glass[3],
                                blend),
                            0.0f,
                            1.0f);
                    }
                    if (execution
                            ->glass_effect_match_appearance_refraction_active) {
                        inst.refraction[0] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_refraction_strength,
                                inst.refraction[0],
                                blend),
                            0.0f,
                            0.35f);
                        inst.refraction[1] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_refraction_edge_bias,
                                inst.refraction[1],
                                blend),
                            0.0f,
                            1.0f);
                        inst.refraction[2] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_refraction_max_offset_pixels,
                                inst.refraction[2],
                                blend),
                            0.0f,
                            8.0f);
                        inst.refraction[3] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_refraction_edge_caustic_intensity,
                                inst.refraction[3],
                                blend),
                            0.0f,
                            0.35f);
                    }
                    if (execution
                            ->glass_effect_match_appearance_dynamic_lighting_active) {
                        inst.lighting[0] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_dynamic_light_direction_x,
                                inst.lighting[0],
                                blend),
                            -0.98f,
                            0.98f);
                        inst.lighting[1] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_dynamic_light_direction_y,
                                inst.lighting[1],
                                blend),
                            -0.98f,
                            0.98f);
                        inst.lighting[2] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_dynamic_light_highlight,
                                inst.lighting[2],
                                blend),
                            0.0f,
                            0.45f);
                        inst.lighting[3] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_dynamic_light_shadow,
                                inst.lighting[3],
                                blend),
                            0.0f,
                            0.36f);
                    }
                    if (execution
                            ->glass_effect_match_appearance_glass_thickness_active) {
                        inst.thickness[0] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_glass_thickness,
                                inst.thickness[0],
                                blend),
                            0.0f,
                            0.78f);
                        inst.thickness[1] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_glass_lensing_gain,
                                inst.thickness[1],
                                blend),
                            1.0f,
                            1.48f);
                        inst.thickness[2] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_glass_shadow_gain,
                                inst.thickness[2],
                                blend),
                            1.0f,
                            1.44f);
                        inst.thickness[3] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_glass_scattering_gain,
                                inst.thickness[3],
                                blend),
                            1.0f,
                            1.40f);
                    }
                    if (execution
                            ->glass_effect_match_appearance_glass_dispersion_active) {
                        inst.dispersion[0] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_glass_dispersion_axial_offset,
                                inst.dispersion[0],
                                blend),
                            0.0f,
                            3.20f);
                        inst.dispersion[1] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_glass_dispersion_tangential_offset,
                                inst.dispersion[1],
                                blend),
                            0.0f,
                            2.45f);
                        inst.dispersion[2] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_glass_dispersion_prismatic_gain,
                                inst.dispersion[2],
                                blend),
                            1.0f,
                            1.75f);
                        inst.dispersion[3] = std::clamp(
                            material_lerp(
                                execution
                                    ->glass_effect_match_appearance_glass_dispersion_caustic_spread,
                                inst.dispersion[3],
                                blend),
                            0.0f,
                            0.40f);
                    }
                }
                if (execution->group_appearance_tint_active) {
                    inst.tint[0] = std::clamp(
                        execution->group_appearance_tint_r,
                        0.0f,
                        1.0f);
                    inst.tint[1] = std::clamp(
                        execution->group_appearance_tint_g,
                        0.0f,
                        1.0f);
                    inst.tint[2] = std::clamp(
                        execution->group_appearance_tint_b,
                        0.0f,
                        1.0f);
                    inst.tint[3] = std::clamp(
                        execution->group_appearance_tint_a,
                        0.0f,
                        1.0f);
                }
                if (execution->group_appearance_spectral_tint_active) {
                    inst.spectral_tint[0] = std::clamp(
                        std::max(
                            inst.spectral_tint[0],
                            execution
                                ->group_appearance_spectral_tint_warmth),
                        0.0f,
                        0.22f);
                    inst.spectral_tint[1] = std::clamp(
                        std::max(
                            inst.spectral_tint[1],
                            execution
                                ->group_appearance_spectral_tint_coolness),
                        0.0f,
                        0.22f);
                    inst.spectral_tint[2] = std::clamp(
                        std::max(
                            inst.spectral_tint[2],
                            execution
                                ->group_appearance_spectral_tint_dispersion),
                        0.0f,
                        0.22f);
                    inst.spectral_tint[3] = std::clamp(
                        std::max(
                            inst.spectral_tint[3],
                            execution->group_appearance_spectral_tint_rim),
                        0.0f,
                        0.28f);
                }
                if (execution->group_appearance_prominent_glass_active) {
                    inst.prominent_glass[0] = std::clamp(
                        std::max(
                            inst.prominent_glass[0],
                            execution
                                ->group_appearance_prominent_glass_intensity),
                        0.0f,
                        1.0f);
                    inst.prominent_glass[1] = std::clamp(
                        std::max(
                            inst.prominent_glass[1],
                            execution
                                ->group_appearance_prominent_glass_tint_weight),
                        0.0f,
                        0.74f);
                    inst.prominent_glass[2] = std::clamp(
                        std::max(
                            inst.prominent_glass[2],
                            execution
                                ->group_appearance_prominent_glass_edge_lift),
                        0.0f,
                        0.30f);
                    inst.prominent_glass[3] = std::clamp(
                        std::max(
                            inst.prominent_glass[3],
                            execution
                                ->group_appearance_prominent_glass_lensing_gain),
                        1.0f,
                        1.36f);
                }
                if (execution->group_appearance_clear_glass_active) {
                    inst.clear_glass[0] = std::clamp(
                        std::max(
                            inst.clear_glass[0],
                            execution->group_appearance_clear_glass_dimming),
                        0.0f,
                        0.34f);
                    inst.clear_glass[1] = std::clamp(
                        std::max(
                            inst.clear_glass[1],
                            execution->group_appearance_clear_glass_contrast),
                        0.0f,
                        0.28f);
                    inst.clear_glass[2] = std::clamp(
                        std::max(
                            inst.clear_glass[2],
                            execution
                                ->group_appearance_clear_glass_brightness_response),
                        0.0f,
                        1.0f);
                    inst.clear_glass[3] = std::clamp(
                        std::max(
                            inst.clear_glass[3],
                            execution
                                ->group_appearance_clear_glass_detail_response),
                        0.0f,
                        1.0f);
                }
                if (execution->group_appearance_refraction_active) {
                    inst.refraction[0] = std::clamp(
                        std::max(
                            inst.refraction[0],
                            execution->group_appearance_refraction_strength),
                        0.0f,
                        0.35f);
                    inst.refraction[1] = std::clamp(
                        std::max(
                            inst.refraction[1],
                            execution->group_appearance_refraction_edge_bias),
                        0.0f,
                        1.0f);
                    inst.refraction[2] = std::clamp(
                        std::max(
                            inst.refraction[2],
                            execution
                                ->group_appearance_refraction_max_offset_pixels),
                        0.0f,
                        8.0f);
                    inst.refraction[3] = std::clamp(
                        std::max(
                            inst.refraction[3],
                            execution
                                ->group_appearance_refraction_edge_caustic_intensity),
                        0.0f,
                        0.35f);
                }
                if (execution->group_appearance_dynamic_lighting_active) {
                    inst.lighting[0] =
                        execution
                            ->group_appearance_dynamic_light_direction_x;
                    inst.lighting[1] =
                        execution
                            ->group_appearance_dynamic_light_direction_y;
                    inst.lighting[2] = std::clamp(
                        std::max(
                            inst.lighting[2],
                            execution
                                ->group_appearance_dynamic_light_highlight),
                        0.0f,
                        0.45f);
                    inst.lighting[3] = std::clamp(
                        std::max(
                            inst.lighting[3],
                            execution
                                ->group_appearance_dynamic_light_shadow),
                        0.0f,
                        0.36f);
                }
                if (execution->group_appearance_glass_thickness_active) {
                    inst.thickness[0] = std::clamp(
                        std::max(
                            inst.thickness[0],
                            execution->group_appearance_glass_thickness),
                        0.0f,
                        0.78f);
                    inst.thickness[1] = std::clamp(
                        std::max(
                            inst.thickness[1],
                            execution
                                ->group_appearance_glass_lensing_gain),
                        1.0f,
                        1.48f);
                    inst.thickness[2] = std::clamp(
                        std::max(
                            inst.thickness[2],
                            execution
                                ->group_appearance_glass_shadow_gain),
                        1.0f,
                        1.44f);
                    inst.thickness[3] = std::clamp(
                        std::max(
                            inst.thickness[3],
                            execution
                                ->group_appearance_glass_scattering_gain),
                        1.0f,
                        1.40f);
                }
                if (execution->group_appearance_glass_dispersion_active) {
                    inst.dispersion[0] = std::clamp(
                        std::max(
                            inst.dispersion[0],
                            execution
                                ->group_appearance_glass_dispersion_axial_offset),
                        0.0f,
                        3.20f);
                    inst.dispersion[1] = std::clamp(
                        std::max(
                            inst.dispersion[1],
                            execution
                                ->group_appearance_glass_dispersion_tangential_offset),
                        0.0f,
                        2.45f);
                    inst.dispersion[2] = std::clamp(
                        std::max(
                            inst.dispersion[2],
                            execution
                                ->group_appearance_glass_dispersion_prismatic_gain),
                        1.0f,
                        1.75f);
                    inst.dispersion[3] = std::clamp(
                        std::max(
                            inst.dispersion[3],
                            execution
                                ->group_appearance_glass_dispersion_caustic_spread),
                        0.0f,
                        0.40f);
                }
                if (execution->overlap_response_active) {
                    auto const overlap = std::clamp(
                        execution->overlap_response_strength,
                        0.0f,
                        1.0f);
                    inst.refraction[0] = std::clamp(
                        inst.refraction[0] * (1.0f - 0.10f * overlap),
                        0.0f,
                        0.35f);
                    inst.refraction[2] = std::clamp(
                        inst.refraction[2] * (1.0f - 0.12f * overlap),
                        0.0f,
                        5.0f);
                    inst.edge_optics[1] = std::clamp(
                        inst.edge_optics[1] + 0.030f * overlap,
                        0.0f,
                        0.85f);
                    inst.edge_optics[2] = std::clamp(
                        inst.edge_optics[2] + 0.024f * overlap,
                        0.0f,
                        0.60f);
                    inst.clear_glass[0] = std::clamp(
                        std::max(inst.clear_glass[0], 0.070f * overlap),
                        0.0f,
                        0.34f);
                    inst.clear_glass[1] = std::clamp(
                        std::max(inst.clear_glass[1], 0.052f * overlap),
                        0.0f,
                        0.28f);
                    inst.thickness[3] = std::clamp(
                        inst.thickness[3] + 0.050f * overlap,
                        1.0f,
                        1.40f);
                }
                if (container_motion.active) {
                    inst.refraction[0] = std::clamp(
                        inst.refraction[0]
                            * (1.0f + 0.18f * container_motion.strength),
                        0.0f,
                        0.35f);
                    inst.refraction[2] = std::clamp(
                        inst.refraction[2] * container_motion.refraction_gain
                            + 0.12f * container_motion.strength,
                        0.0f,
                        5.0f);
                    inst.refraction[3] = std::clamp(
                        inst.refraction[3] * container_motion.caustic_gain
                            + 0.035f * container_motion.strength,
                        0.0f,
                        0.35f);
                    inst.edge_optics[0] = std::clamp(
                        std::max(
                            inst.edge_optics[0],
                            record->plan.edge_optics.bevel_width
                                + 0.80f * container_motion.strength),
                        0.0f,
                        16.0f);
                    inst.edge_optics[1] = std::clamp(
                        inst.edge_optics[1]
                            * (1.0f + 0.40f * container_motion.strength)
                            + 0.040f * container_motion.strength,
                        0.0f,
                        0.85f);
                    inst.edge_optics[2] = std::clamp(
                        inst.edge_optics[2]
                            * (1.0f + 0.32f * container_motion.strength)
                            + 0.030f * container_motion.strength,
                        0.0f,
                        0.60f);
                    inst.edge_optics[3] = std::clamp(
                        inst.edge_optics[3]
                            + 0.025f * container_motion.strength,
                        0.0f,
                        0.16f);
                    inst.spectral_tint[2] = std::clamp(
                        inst.spectral_tint[2]
                            + 0.030f * container_motion.strength,
                        0.0f,
                        0.22f);
                    inst.spectral_tint[3] = std::clamp(
                        inst.spectral_tint[3]
                            + 0.040f * container_motion.strength,
                        0.0f,
                        0.28f);
                    inst.lighting[2] = std::clamp(
                        inst.lighting[2]
                            + 0.055f * container_motion.strength,
                        0.0f,
                        0.45f);
                    inst.lighting[3] = std::clamp(
                        inst.lighting[3]
                            + 0.040f * container_motion.strength,
                        0.0f,
                        0.36f);
                    inst.thickness[0] = std::clamp(
                        inst.thickness[0]
                            + 0.080f * container_motion.strength,
                        0.0f,
                        0.78f);
                    inst.thickness[1] = std::clamp(
                        inst.thickness[1]
                            + 0.070f * container_motion.strength,
                        1.0f,
                        1.48f);
                    inst.thickness[2] = std::clamp(
                        inst.thickness[2]
                            + 0.055f * container_motion.strength,
                        1.0f,
                        1.44f);
                    inst.thickness[3] = std::clamp(
                        inst.thickness[3]
                            + 0.050f * container_motion.strength,
                        1.0f,
                        1.40f);
                    inst.dispersion[0] = std::clamp(
                        inst.dispersion[0]
                            + 0.16f * container_motion.strength,
                        0.0f,
                        3.20f);
                    inst.dispersion[1] = std::clamp(
                        inst.dispersion[1]
                            + 0.12f * container_motion.strength,
                        0.0f,
                        2.45f);
                    inst.dispersion[2] = std::clamp(
                        inst.dispersion[2]
                            + 0.08f * container_motion.strength,
                        1.0f,
                        1.75f);
                    inst.dispersion[3] = std::clamp(
                        inst.dispersion[3]
                            + 0.030f * container_motion.strength,
                        0.0f,
                        0.40f);
                }
            } else {
                inst.group_rect[0] = execution->group_x;
                inst.group_rect[1] = execution->group_y;
                inst.group_rect[2] = execution->group_w;
                inst.group_rect[3] = execution->group_h;
            }
            inst.group_effects[0] = execution->shape_blend_strength;
            inst.group_effects[1] =
                execution->inner_edge_alpha_blend_strength;
            inst.group_effects[2] =
                (execution->shape_blend_execution ? 1.0f : 0.0f)
                + (execution->union_execution ? 2.0f : 0.0f)
                + (execution->morph_execution ? 4.0f : 0.0f)
                + (execution->shared_backdrop_scope ? 8.0f : 0.0f)
                + (execution->glass_effect_match_execution ? 16.0f : 0.0f)
                + (execution->group_surface_execution ? 32.0f : 0.0f);
            inst.group_effects[3] = execution->overlap_response_strength;
            inst.fusion_optics[0] = execution->fusion_strength;
            inst.fusion_optics[1] = execution->fusion_lensing_gain;
            inst.fusion_optics[2] = execution->fusion_edge_lift;
            inst.fusion_optics[3] = execution->fusion_shadow_gain;
        }
    }
}

inline float material_paint_layer_alpha(
        MaterialPaintLayer const& layer) noexcept {
    auto const color_alpha = layer.color.a / 255.0f;
    if (material_paint_layer_matches(layer.executor, "rounded-fill"))
        return std::max(color_alpha, layer.opacity);
    return color_alpha * layer.opacity;
}

inline bool configure_material_paint_layer_instance(
        ColorInstanceGPU& inst,
        MaterialPlan const& plan,
        MaterialPaintLayer const& layer,
        MaterialContainerExecutionDescriptor const* execution = nullptr) {
    auto const geometry =
        material_paint_layer_execution_geometry(plan, layer, execution);
    if (!geometry.active)
        return false;
    auto const rounded_edge =
        material_paint_layer_matches(layer.executor, "rounded-edge");
    inst.rect[0] = geometry.x;
    inst.rect[1] = geometry.y;
    inst.rect[2] = geometry.w;
    inst.rect[3] = geometry.h;
    inst.color[0] = layer.color.r / 255.0f;
    inst.color[1] = layer.color.g / 255.0f;
    inst.color[2] = layer.color.b / 255.0f;
    inst.color[3] = material_paint_layer_alpha(layer);
    inst.params[0] = geometry.radius;
    inst.params[1] = rounded_edge ? std::max(layer.stroke_width, 0.5f) : 0.0f;
    inst.params[2] =
        rounded_edge ? 3.0f : (layer.soft_edge_radius > 0.0f ? 5.0f : 2.0f);
    inst.params[3] = layer.soft_edge_radius;
    return true;
}

inline void append_material_paint_layer_instance(
        std::vector<ColorInstanceGPU>& out,
        MaterialPlan const& plan,
        MaterialPaintLayer const& layer) {
    ColorInstanceGPU inst{};
    if (!configure_material_paint_layer_instance(inst, plan, layer))
        return;
    out.push_back(inst);
}

inline void append_material_paint_layer_instances(
        std::vector<ColorInstanceGPU>& out,
        MaterialPlan const& plan) {
    for (unsigned int i = 0; i < plan.paint_layer_count; ++i)
        append_material_paint_layer_instance(out, plan, plan.paint_layers[i]);
}

inline MaterialRuntimeRecord const* find_material_runtime_record(
        std::span<MaterialRuntimeRecord const> records,
        std::uint32_t command_index) {
    for (auto const& record : records) {
        if (record.command_index == command_index)
            return &record;
    }
    return nullptr;
}

inline MaterialContainerExecutionDescriptor const*
find_material_container_execution_descriptor(
        std::span<MaterialContainerExecutionDescriptor const> descriptors,
        std::uint32_t command_index) {
    for (auto const& descriptor : descriptors) {
        if (descriptor.command_index == command_index)
            return &descriptor;
    }
    return nullptr;
}

inline void apply_material_paint_layer_execution_range(
        std::vector<ColorInstanceGPU>& instances,
        MaterialPlan const& plan,
        MaterialContainerExecutionDescriptor const* execution,
        std::size_t first,
        std::size_t count) {
    auto cursor = first;
    auto const end = std::min(first + count, instances.size());
    for (unsigned int i = 0; i < plan.paint_layer_count && cursor < end; ++i) {
        auto const& layer = plan.paint_layers[i];
        if (!material_paint_layer_execution_geometry(plan, layer).active)
            continue;
        if (!configure_material_paint_layer_instance(
                instances[cursor],
                plan,
                layer,
                execution)) {
            instances[cursor].color[3] = 0.0f;
        }
        ++cursor;
    }
    while (cursor < end) {
        instances[cursor].color[3] = 0.0f;
        ++cursor;
    }
}

inline void apply_material_paint_layer_execution_ranges(FrameScratch& scratch) {
    if (scratch.material_paint_layer_ranges.empty()
        || scratch.material_container_execution_descriptors.empty()) {
        return;
    }
    for (auto const& range : scratch.material_paint_layer_ranges) {
        if (range.batch_index >= scratch.batches.size())
            continue;
        auto const* record = find_material_runtime_record(
            scratch.material_records,
            range.command_index);
        auto const* execution = find_material_container_execution_descriptor(
            scratch.material_container_execution_descriptors,
            range.command_index);
        if (!record || !execution)
            continue;
        auto const flat_first =
            static_cast<std::size_t>(
                scratch.batches[range.batch_index].color_first)
            + range.first;
        apply_material_paint_layer_execution_range(
            scratch.color_instances,
            record->plan,
            execution,
            flat_first,
            range.count);
    }
}

// FillPath helpers. Walk a flat polygon (vertex list with an implicit
// close) and ear-clip it into a triangle list. The triangles are then
// fed into the dedicated triangle pipeline (`vs_tri` / `fs_tri`) as
// raw 3-vertex tuples — hardware rasterisation gives pixel-perfect
// coverage on shared edges and collapses tens of thousands of HATCH
// fills (e.g. colorwh.dwg's 36 095 boundary loops) into a single
// drawPrimitives call.
//
// The earlier CPU scanline path emitted one ColorInstance per
// 1-pixel-tall horizontal strip per triangle, which both (a) produced
// sub-pixel gaps at slim hatch tips when the strip width fell below
// 0.5 px and the strip was dropped, and (b) ballooned the colour
// instance buffer past tens of MB on dense CAD content.
inline bool point_in_triangle(float px, float py,
                              float ax, float ay,
                              float bx, float by,
                              float cx, float cy) {
    // Sign-of-cross-products test. CCW polygon = all three crosses
    // share the same sign for an interior point.
    float const d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by);
    float const d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy);
    float const d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay);
    bool const has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool const has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(has_neg && has_pos);
}

// Ear-clip triangulation. Writes 6 floats per triangle (v0x, v0y,
// v1x, v1y, v2x, v2y) appended to `tris`; the caller is responsible
// for any reset / size-tracking. `poly` is taken by reference and
// MAY be reordered in place (orientation normalisation). `remain` is
// caller-owned scratch space — passing it in avoids a per-call
// std::vector allocation that becomes 36k+ allocs/frame on
// HATCH-heavy DWGs (colorwh.dwg).
inline void polygon_ear_clip(std::vector<float>& poly,
                             std::vector<float>& tris,
                             std::vector<std::size_t>& remain) {
    if (poly.size() < 6) return;  // need ≥ 3 vertices

    std::size_t const n0 = poly.size() / 2;

    // Signed area × 2 to detect orientation. CCW > 0; CW < 0.
    double area2 = 0.0;
    for (std::size_t i = 0; i < n0; ++i) {
        std::size_t const j = (i + 1) % n0;
        area2 += static_cast<double>(poly[2 * i])     * poly[2 * j + 1]
              -  static_cast<double>(poly[2 * j])     * poly[2 * i + 1];
    }
    if (std::fabs(area2) < 1e-9) return;

    // Normalise to CCW so the convex-vertex test below has a stable sign.
    if (area2 < 0.0) {
        for (std::size_t i = 0; i < n0 / 2; ++i) {
            std::swap(poly[2 * i],     poly[2 * (n0 - 1 - i)]);
            std::swap(poly[2 * i + 1], poly[2 * (n0 - 1 - i) + 1]);
        }
    }

    // Indices into `poly` (vertex indices, not float offsets).
    remain.clear();
    remain.reserve(n0);
    for (std::size_t i = 0; i < n0; ++i) remain.push_back(i);

    // Convex fast path: triangle-fan from vertex 0. Most CAD HATCH
    // boundaries (wedges, rectangles, regular polygons) are convex,
    // and the dominant colorwh.dwg case fits this. Skip the
    // O(N²) ear search when no reflex vertex exists.
    bool convex = true;
    for (std::size_t i = 0; i < n0 && convex; ++i) {
        std::size_t const ip = (i + n0 - 1) % n0;
        std::size_t const ic = i;
        std::size_t const in = (i + 1) % n0;
        float const ax = poly[2 * ip], ay = poly[2 * ip + 1];
        float const bx = poly[2 * ic], by = poly[2 * ic + 1];
        float const cx = poly[2 * in], cy = poly[2 * in + 1];
        float const cross = (bx - ax) * (cy - ay)
                          - (by - ay) * (cx - ax);
        if (cross < 0.0f) convex = false;
    }
    if (convex) {
        float const ax = poly[0], ay = poly[1];
        for (std::size_t i = 1; i + 1 < n0; ++i) {
            float const bx = poly[2 * i],     by = poly[2 * i + 1];
            float const cx = poly[2 * (i+1)], cy = poly[2 * (i+1) + 1];
            tris.push_back(ax); tris.push_back(ay);
            tris.push_back(bx); tris.push_back(by);
            tris.push_back(cx); tris.push_back(cy);
        }
        return;
    }

    // Safety bound: at most n triangles for n vertices, but loop more
    // generously to tolerate near-degenerate ears.
    int safety = static_cast<int>(n0) * 3 + 1;
    while (remain.size() >= 3 && safety-- > 0) {
        bool found = false;
        std::size_t const m = remain.size();
        for (std::size_t i = 0; i < m; ++i) {
            std::size_t const ip = remain[(i + m - 1) % m];
            std::size_t const ic = remain[i];
            std::size_t const in = remain[(i + 1) % m];
            float const ax = poly[2 * ip], ay = poly[2 * ip + 1];
            float const bx = poly[2 * ic], by = poly[2 * ic + 1];
            float const cx = poly[2 * in], cy = poly[2 * in + 1];
            // Convex (CCW) test: cross > 0.
            float const cross = (bx - ax) * (cy - ay)
                              - (by - ay) * (cx - ax);
            if (cross <= 0.0f) continue;  // reflex / colinear
            // Empty test: no other remaining vertex strictly inside abc.
            bool empty = true;
            for (std::size_t k : remain) {
                if (k == ip || k == ic || k == in) continue;
                float const px = poly[2 * k], py = poly[2 * k + 1];
                if (point_in_triangle(px, py, ax, ay, bx, by, cx, cy)) {
                    empty = false; break;
                }
            }
            if (!empty) continue;
            tris.push_back(ax); tris.push_back(ay);
            tris.push_back(bx); tris.push_back(by);
            tris.push_back(cx); tris.push_back(cy);
            remain.erase(remain.begin() + static_cast<long>(i));
            found = true;
            break;
        }
        if (!found) break;  // self-intersecting / degenerate polygon
    }
}


inline void append_text_instance(std::vector<TextInstanceGPU>& out,
                                 float x, float y, float w, float h,
                                 float u, float v, float uw, float vh,
                                 float r, float g, float b, float a,
                                 float pivot_x = 0.0f, float pivot_y = 0.0f,
                                 float cos_t = 1.0f, float sin_t = 0.0f) {
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
    inst.rot[0] = pivot_x;
    inst.rot[1] = pivot_y;
    inst.rot[2] = cos_t;
    inst.rot[3] = sin_t;
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

inline bool fills_logical_viewport(float x, float y, float w, float h,
                                   MaterialEnvironment const& env) noexcept {
    float const scale = env.render_target.scale > 0.0f
        ? env.render_target.scale
        : 1.0f;
    float const logical_width =
        static_cast<float>(env.render_target.width) / scale;
    float const logical_height =
        static_cast<float>(env.render_target.height) / scale;
    constexpr float tolerance = 0.75f;
    return std::fabs(x) <= tolerance
        && std::fabs(y) <= tolerance
        && std::fabs(w - logical_width) <= tolerance
        && std::fabs(h - logical_height) <= tolerance;
}

inline std::filesystem::path resolve_image_path(std::string const& url) {
    return cppx::resource::resolve_path(
        std::filesystem::current_path(),
        std::string_view{url});
}

inline bool decode_frame_commands(unsigned char const* buf, unsigned int len,
                                  float line_height_ratio,
                                  MaterialEnvironment const& material_env,
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
    std::uint32_t command_index = 0;
    while (reader.cur < reader.end) {
        unsigned int raw_cmd = 0;
        if (!reader.read_u32(raw_cmd))
            return false;
        auto const current_command_index = command_index++;

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
                if (color.a == 255
                    && fills_logical_viewport(x, y, w, h, material_env)) {
                    ++scratch.full_frame_opaque_fill_count;
                }
                append_color_instance(
                    scratch.batches.back().colors,
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
                    scratch.batches.back().colors,
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
                    scratch.batches.back().colors,
                    x, y, w, h,
                    color.r / 255.0f, color.g / 255.0f,
                    color.b / 255.0f, color.a / 255.0f,
                    radius, 0.0f, 2.0f);
                break;
            }
            case Cmd::MaterialRect: {
                float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
                float radius = 0.0f;
                unsigned int kind = 0;
                unsigned int role = 0;
                float opacity = 0.0f;
                float blur_radius = 0.0f;
                unsigned int packed = 0;
                float saturation = 1.0f;
                float luminance_floor = 0.0f;
                float luminance_gain = 1.0f;
                float edge_highlight = 0.0f;
                float edge_width = 1.0f;
                float noise_opacity = 0.0f;
                float shadow_alpha = 0.0f;
                float shadow_radius = 0.0f;
                unsigned int container_id = 0;
                unsigned int union_id = 0;
                float container_spacing = 0.0f;
                unsigned int container_flags = 0;
                unsigned int interaction_flags = 0;
                float interaction_x = 0.5f;
                float interaction_y = 0.5f;
                unsigned int transition_kind = 0;
                float transition_progress = 1.0f;
                unsigned int transition_flags = 1u;
                unsigned int glass_namespace_id = 0;
                unsigned int glass_effect_id = 0;
                unsigned int glass_background_kind = 0;
                float glass_background_feather_padding = 0.0f;
                float glass_background_soft_edge_radius = 0.0f;
                unsigned int prominence_flags = 0;
                float prominence_intensity = 1.0f;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(w) || !reader.read_f32(h)
                    || !reader.read_f32(radius)
                    || !reader.read_u32(kind)
                    || !reader.read_u32(role)
                    || !reader.read_f32(opacity)
                    || !reader.read_f32(blur_radius)
                    || !reader.read_u32(packed)
                    || !reader.read_f32(saturation)
                    || !reader.read_f32(luminance_floor)
                    || !reader.read_f32(luminance_gain)
                    || !reader.read_f32(edge_highlight)
                    || !reader.read_f32(edge_width)
                    || !reader.read_f32(noise_opacity)
                    || !reader.read_f32(shadow_alpha)
                    || !reader.read_f32(shadow_radius)
                    || !reader.read_u32(container_id)
                    || !reader.read_u32(union_id)
                    || !reader.read_f32(container_spacing)
                    || !reader.read_u32(container_flags)
                    || !reader.read_u32(interaction_flags)
                    || !reader.read_f32(interaction_x)
                    || !reader.read_f32(interaction_y)
                    || !reader.read_u32(transition_kind)
                    || !reader.read_f32(transition_progress)
                    || !reader.read_u32(transition_flags)
                    || !reader.read_u32(glass_namespace_id)
                    || !reader.read_u32(glass_effect_id)
                    || !reader.read_u32(glass_background_kind)
                    || !reader.read_f32(glass_background_feather_padding)
                    || !reader.read_f32(glass_background_soft_edge_radius)
                    || !reader.read_u32(prominence_flags)
                    || !reader.read_f32(prominence_intensity))
                    return false;
                auto material_env_for_command = material_env;
                material_env_for_command.debug_seed.node =
                    current_command_index;
                MaterialCommandDescriptor descriptor{
                    material_kind_from_wire(kind),
                    material_surface_role_from_wire(role),
                    material_container_descriptor_from_wire(
                        container_id,
                        union_id,
                        container_spacing,
                        container_flags),
                    opacity,
                    blur_radius,
                    unpack_color(packed),
                    saturation,
                    luminance_floor,
                    luminance_gain,
                    edge_highlight,
                    edge_width,
                    noise_opacity,
                    shadow_alpha,
                    shadow_radius,
                    material_interaction_descriptor_from_wire(
                        interaction_flags,
                        interaction_x,
                        interaction_y),
                    material_transition_descriptor_from_wire(
                        transition_kind,
                        transition_progress,
                        transition_flags),
                    material_glass_identity_from_wire(
                        glass_namespace_id,
                        glass_effect_id),
                    material_glass_background_from_wire(
                        glass_background_kind,
                        glass_background_feather_padding,
                        glass_background_soft_edge_radius),
                    material_prominence_from_wire(
                        prominence_flags,
                        prominence_intensity)};
                auto plan = plan_material_surface(
                    material_request_for_command(
                        descriptor,
                        MaterialGeometry{x, y, w, h, radius},
                        ::phenotype::detail::g_app.theme),
                    material_env_for_command);
                scratch.material_records.push_back(
                    MaterialRuntimeRecord{plan, current_command_index});
                auto& cur = scratch.batches.back();
                if (!cur.tri_vertices.empty() || !cur.colors.empty()
                    || !cur.materials.empty() || !cur.arcs.empty()
                    || !cur.images.empty() || !cur.texts.empty()
                    || cur.pending_text_runs) {
                    open_same_scissor_batch(scratch);
                }
                if (material_plan_uses_sampled_backdrop_executor(plan)) {
                    append_material_instance(
                        scratch.batches.back().materials,
                        plan,
                        current_command_index);
                } else {
                    auto const batch_index = scratch.batches.size() - 1u;
                    auto& colors = scratch.batches.back().colors;
                    auto const first = colors.size();
                    append_material_paint_layer_instances(
                        colors,
                        plan);
                    auto const count = colors.size() - first;
                    if (count > 0u) {
                        scratch.material_paint_layer_ranges.push_back(
                            MaterialPaintLayerRange{
                                current_command_index,
                                batch_index,
                                first,
                                count});
                    }
                }
                open_same_scissor_batch(scratch);
                break;
            }
            case Cmd::DrawText: {
                float x = 0.0f, y = 0.0f, font_size = 0.0f, rotation = 0.0f;
                float width_factor = 1.0f;
                unsigned int flags = 0;
                unsigned int packed = 0;
                unsigned int family_len = 0;
                unsigned int text_len = 0;
                char const* family = nullptr;
                char const* text = nullptr;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(font_size)
                    || !reader.read_f32(rotation)
                    || !reader.read_f32(width_factor)
                    || !reader.read_u32(flags)
                    || !reader.read_u32(packed) || !reader.read_u32(family_len)
                    || !reader.read_text(family, family_len)
                    || !reader.read_u32(text_len)
                    || !reader.read_text(text, text_len))
                    return false;
                auto color = unpack_color(packed);
                auto foreground = material_resolve_text_foreground(
                    scratch.material_records,
                    current_command_index,
                    x,
                    y,
                    color,
                    ::phenotype::detail::g_app.theme);
                if (foreground.has_material)
                    ++scratch.foreground_text_candidate_count;
                if (foreground.remapped) {
                    ++scratch.foreground_text_remap_count;
                    color = foreground.color;
                }
                ParsedTextRun run{};
                run.x = x;
                run.y = y;
                run.font_size = font_size;
                run.rotation = rotation;
                run.width_factor = width_factor;
                run.mono = (flags & 1u) != 0;
                run.r = color.r / 255.0f;
                run.g = color.g / 255.0f;
                run.b = color.b / 255.0f;
                run.a = color.a / 255.0f;
                run.line_height = font_size * line_height_ratio;
                run.text = text;
                run.len = text_len;
                run.batch_idx = static_cast<std::uint32_t>(scratch.batches.size() - 1);
                if (family && family_len > 0)
                    run.font_key.family.assign(family, family_len);
                run.font_key.weight = (flags & 2u)
                    ? ::phenotype::FontWeight::Bold
                    : ::phenotype::FontWeight::Regular;
                run.font_key.style  = (flags & 4u)
                    ? ::phenotype::FontStyle::Italic
                    : ::phenotype::FontStyle::Upright;
                run.font_key.mono = run.mono;
                scratch.batches.back().pending_text_runs = true;
                scratch.text_runs.push_back(std::move(run));
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
                append_stroked_segment(
                    scratch,
                    x1, y1, x2, y2, thickness,
                    color.r / 255.0f,
                    color.g / 255.0f,
                    color.b / 255.0f,
                    color.a / 255.0f);
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
                image.batch_idx =
                    static_cast<std::uint32_t>(scratch.batches.size() - 1);
                scratch.images.push_back(std::move(image));
                break;
            }
            case Cmd::Scissor: {
                float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(w) || !reader.read_f32(h))
                    return false;
                open_scissor_batch(scratch, x, y, w, h);
                break;
            }
            case Cmd::DrawArc: {
                float cx = 0.0f, cy = 0.0f, r = 0.0f;
                float sa = 0.0f, ea = 0.0f, th = 0.0f;
                unsigned int packed = 0;
                if (!reader.read_f32(cx) || !reader.read_f32(cy)
                    || !reader.read_f32(r)
                    || !reader.read_f32(sa) || !reader.read_f32(ea)
                    || !reader.read_f32(th)
                    || !reader.read_u32(packed))
                    return false;
                if (r <= 0.0f) break;
                auto color = unpack_color(packed);
                ArcInstanceGPU inst{};
                inst.center_radius_thickness[0] = cx;
                inst.center_radius_thickness[1] = cy;
                inst.center_radius_thickness[2] = r;
                inst.center_radius_thickness[3] = th;
                inst.angles[0] = sa;
                inst.angles[1] = ea;
                inst.color[0]  = color.r / 255.0f;
                inst.color[1]  = color.g / 255.0f;
                inst.color[2]  = color.b / 255.0f;
                inst.color[3]  = color.a / 255.0f;
                scratch.batches.back().arcs.push_back(inst);
                break;
            }
            case Cmd::Path: {
                // Walk the verb stream and dispatch each segment onto
                // the existing instance buffers. Axis-aligned strokes
                // use the color SDF path, diagonal strokes use triangle
                // bodies plus round color caps, and ArcTo pushes an
                // `ArcInstanceGPU` directly. No new pipeline is introduced;
                // stroke rendering reuses the color, triangle, and arc
                // backends already in flight.
                float thickness = 0.0f;
                unsigned int packed = 0;
                unsigned int verb_count = 0;
                if (!reader.read_f32(thickness)
                    || !reader.read_u32(packed)
                    || !reader.read_u32(verb_count))
                    return false;
                auto color = unpack_color(packed);
                float const cr = color.r / 255.0f;
                float const cg = color.g / 255.0f;
                float const cb = color.b / 255.0f;
                float const ca = color.a / 255.0f;

                float pen_x = 0.0f, pen_y = 0.0f;
                float sub_x = 0.0f, sub_y = 0.0f;
                bool has_pen = false;

                bool triangle_stroke_batch_ready = false;
                auto emit_segment = [&](float x1, float y1,
                                        float x2, float y2) {
                    if (stroked_segment_needs_triangle_body(
                            x1, y1, x2, y2, thickness)) {
                        if (!triangle_stroke_batch_ready) {
                            ensure_triangle_order_batch(scratch);
                            triangle_stroke_batch_ready = true;
                        }
                    }
                    append_stroked_segment_to_batch(
                        scratch.batches.back(),
                        x1, y1, x2, y2,
                        thickness,
                        cr, cg, cb, ca);
                };

                // Recursive De Casteljau flatten — split until each
                // control point sits within `flatness = 0.5px` of the
                // chord, or until depth runs out (cap at 16 levels =
                // up to 65 536 segments per curve, which is far more
                // than any visible CAD input requires).
                constexpr float flatness2 = 0.25f;  // (0.5px)^2
                constexpr int max_depth = 16;

                auto flatten_quad = [&](auto& self,
                                        float p0x, float p0y,
                                        float p1x, float p1y,
                                        float p2x, float p2y,
                                        int depth) -> void {
                    float lx = p2x - p0x;
                    float ly = p2y - p0y;
                    float llen2 = lx * lx + ly * ly;
                    bool flat = (depth == 0);
                    if (!flat && llen2 > 1e-6f) {
                        float d = (p1x - p0x) * ly - (p1y - p0y) * lx;
                        if (d * d < flatness2 * llen2) flat = true;
                    }
                    if (flat || llen2 < 1e-6f) {
                        emit_segment(p0x, p0y, p2x, p2y);
                        return;
                    }
                    float a0x = (p0x + p1x) * 0.5f;
                    float a0y = (p0y + p1y) * 0.5f;
                    float a1x = (p1x + p2x) * 0.5f;
                    float a1y = (p1y + p2y) * 0.5f;
                    float midx = (a0x + a1x) * 0.5f;
                    float midy = (a0y + a1y) * 0.5f;
                    self(self, p0x, p0y, a0x, a0y, midx, midy, depth - 1);
                    self(self, midx, midy, a1x, a1y, p2x, p2y, depth - 1);
                };

                auto flatten_cubic = [&](auto& self,
                                         float p0x, float p0y,
                                         float p1x, float p1y,
                                         float p2x, float p2y,
                                         float p3x, float p3y,
                                         int depth) -> void {
                    float lx = p3x - p0x;
                    float ly = p3y - p0y;
                    float llen2 = lx * lx + ly * ly;
                    bool flat = (depth == 0);
                    if (!flat && llen2 > 1e-6f) {
                        float d1 = (p1x - p0x) * ly - (p1y - p0y) * lx;
                        float d2 = (p2x - p0x) * ly - (p2y - p0y) * lx;
                        if (d1 * d1 < flatness2 * llen2
                            && d2 * d2 < flatness2 * llen2) {
                            flat = true;
                        }
                    }
                    if (flat || llen2 < 1e-6f) {
                        emit_segment(p0x, p0y, p3x, p3y);
                        return;
                    }
                    float a01x = (p0x + p1x) * 0.5f;
                    float a01y = (p0y + p1y) * 0.5f;
                    float a12x = (p1x + p2x) * 0.5f;
                    float a12y = (p1y + p2y) * 0.5f;
                    float a23x = (p2x + p3x) * 0.5f;
                    float a23y = (p2y + p3y) * 0.5f;
                    float b01x = (a01x + a12x) * 0.5f;
                    float b01y = (a01y + a12y) * 0.5f;
                    float b12x = (a12x + a23x) * 0.5f;
                    float b12y = (a12y + a23y) * 0.5f;
                    float midx = (b01x + b12x) * 0.5f;
                    float midy = (b01y + b12y) * 0.5f;
                    self(self,
                         p0x, p0y, a01x, a01y, b01x, b01y, midx, midy,
                         depth - 1);
                    self(self,
                         midx, midy, b12x, b12y, a23x, a23y, p3x, p3y,
                         depth - 1);
                };

                for (unsigned int i = 0; i < verb_count; ++i) {
                    unsigned int verb = 0;
                    if (!reader.read_u32(verb)) return false;
                    switch (static_cast<PathVerb>(verb)) {
                    case PathVerb::MoveTo: {
                        float x = 0.0f, y = 0.0f;
                        if (!reader.read_f32(x) || !reader.read_f32(y))
                            return false;
                        pen_x = x; pen_y = y;
                        sub_x = x; sub_y = y;
                        has_pen = true;
                        break;
                    }
                    case PathVerb::LineTo: {
                        float x = 0.0f, y = 0.0f;
                        if (!reader.read_f32(x) || !reader.read_f32(y))
                            return false;
                        if (has_pen) emit_segment(pen_x, pen_y, x, y);
                        pen_x = x; pen_y = y;
                        has_pen = true;
                        break;
                    }
                    case PathVerb::QuadTo: {
                        float c1x = 0.0f, c1y = 0.0f;
                        float x = 0.0f, y = 0.0f;
                        if (!reader.read_f32(c1x) || !reader.read_f32(c1y)
                            || !reader.read_f32(x) || !reader.read_f32(y))
                            return false;
                        if (has_pen) {
                            flatten_quad(flatten_quad,
                                         pen_x, pen_y, c1x, c1y, x, y,
                                         max_depth);
                        }
                        pen_x = x; pen_y = y;
                        has_pen = true;
                        break;
                    }
                    case PathVerb::CubicTo: {
                        float c1x = 0.0f, c1y = 0.0f;
                        float c2x = 0.0f, c2y = 0.0f;
                        float x = 0.0f, y = 0.0f;
                        if (!reader.read_f32(c1x) || !reader.read_f32(c1y)
                            || !reader.read_f32(c2x) || !reader.read_f32(c2y)
                            || !reader.read_f32(x) || !reader.read_f32(y))
                            return false;
                        if (has_pen) {
                            flatten_cubic(flatten_cubic,
                                          pen_x, pen_y, c1x, c1y,
                                          c2x, c2y, x, y, max_depth);
                        }
                        pen_x = x; pen_y = y;
                        has_pen = true;
                        break;
                    }
                    case PathVerb::ArcTo: {
                        float acx = 0.0f, acy = 0.0f, ar = 0.0f;
                        float asa = 0.0f, aea = 0.0f;
                        if (!reader.read_f32(acx) || !reader.read_f32(acy)
                            || !reader.read_f32(ar)
                            || !reader.read_f32(asa) || !reader.read_f32(aea))
                            return false;
                        if (ar > 0.0f) {
                            ArcInstanceGPU inst{};
                            inst.center_radius_thickness[0] = acx;
                            inst.center_radius_thickness[1] = acy;
                            inst.center_radius_thickness[2] = ar;
                            inst.center_radius_thickness[3] = thickness;
                            inst.angles[0] = asa;
                            inst.angles[1] = aea;
                            inst.color[0] = cr;
                            inst.color[1] = cg;
                            inst.color[2] = cb;
                            inst.color[3] = ca;
                            scratch.batches.back().arcs.push_back(inst);
                        }
                        // Path-spec semantics for the pen after an ArcTo
                        // are out of scope here — cad++ chains ArcTo
                        // segments via explicit MoveTo/LineTo when
                        // continuity matters. Leave the pen position
                        // unchanged; future PRs can refine.
                        break;
                    }
                    case PathVerb::Close: {
                        if (has_pen)
                            emit_segment(pen_x, pen_y, sub_x, sub_y);
                        pen_x = sub_x;
                        pen_y = sub_y;
                        break;
                    }
                    default:
                        return false;
                    }
                }
                break;
            }
            case Cmd::FillPath: {
                // Walk the verb stream into a flat polygon, ear-clip
                // it into triangles, then push 3 raw vertices per
                // triangle into the batch's tri_vertices for the
                // dedicated triangle pipeline. Polygon and triangle
                // scratch buffers live on FrameScratch and are
                // cleared (not destroyed) per FillPath, so a frame
                // with 36 095 HATCHes pays for the buffer's high-
                // water mark once instead of allocating 100k+ vectors.
                //
                // Single closed loop only. Self-intersection / multi-
                // loop / hole semantics are out of scope; cad++ HATCH
                // emits one boundary loop per `Painter::fill_path`
                // call so this matches the use case.
                unsigned int packed = 0;
                unsigned int verb_count = 0;
                if (!reader.read_u32(packed)
                    || !reader.read_u32(verb_count))
                    return false;
                auto color = unpack_color(packed);
                float const cr = color.r / 255.0f;
                float const cg = color.g / 255.0f;
                float const cb = color.b / 255.0f;
                float const ca = color.a / 255.0f;

                auto& polygon = scratch.fill_polygon;
                polygon.clear();
                polygon.reserve(verb_count * 2);
                auto append = [&](float x, float y) {
                    polygon.push_back(x);
                    polygon.push_back(y);
                };

                constexpr float flatness2 = 0.25f;  // (0.5 px)^2
                constexpr int max_depth = 16;
                auto flatten_quad = [&](auto& self,
                                        float p0x, float p0y,
                                        float p1x, float p1y,
                                        float p2x, float p2y,
                                        int depth) -> void {
                    float lx = p2x - p0x, ly = p2y - p0y;
                    float llen2 = lx * lx + ly * ly;
                    bool flat = (depth == 0);
                    if (!flat && llen2 > 1e-6f) {
                        float d = (p1x - p0x) * ly - (p1y - p0y) * lx;
                        if (d * d < flatness2 * llen2) flat = true;
                    }
                    if (flat || llen2 < 1e-6f) {
                        append(p2x, p2y); return;
                    }
                    float a0x = (p0x + p1x) * 0.5f, a0y = (p0y + p1y) * 0.5f;
                    float a1x = (p1x + p2x) * 0.5f, a1y = (p1y + p2y) * 0.5f;
                    float mx  = (a0x + a1x) * 0.5f, my  = (a0y + a1y) * 0.5f;
                    self(self, p0x, p0y, a0x, a0y, mx, my, depth - 1);
                    self(self, mx, my, a1x, a1y, p2x, p2y, depth - 1);
                };
                auto flatten_cubic = [&](auto& self,
                                         float p0x, float p0y,
                                         float p1x, float p1y,
                                         float p2x, float p2y,
                                         float p3x, float p3y,
                                         int depth) -> void {
                    float lx = p3x - p0x, ly = p3y - p0y;
                    float llen2 = lx * lx + ly * ly;
                    bool flat = (depth == 0);
                    if (!flat && llen2 > 1e-6f) {
                        float d1 = (p1x - p0x) * ly - (p1y - p0y) * lx;
                        float d2 = (p2x - p0x) * ly - (p2y - p0y) * lx;
                        if (d1 * d1 < flatness2 * llen2
                            && d2 * d2 < flatness2 * llen2) flat = true;
                    }
                    if (flat || llen2 < 1e-6f) {
                        append(p3x, p3y); return;
                    }
                    float a01x = (p0x + p1x) * 0.5f, a01y = (p0y + p1y) * 0.5f;
                    float a12x = (p1x + p2x) * 0.5f, a12y = (p1y + p2y) * 0.5f;
                    float a23x = (p2x + p3x) * 0.5f, a23y = (p2y + p3y) * 0.5f;
                    float b01x = (a01x + a12x) * 0.5f, b01y = (a01y + a12y) * 0.5f;
                    float b12x = (a12x + a23x) * 0.5f, b12y = (a12y + a23y) * 0.5f;
                    float mx   = (b01x + b12x) * 0.5f, my   = (b01y + b12y) * 0.5f;
                    self(self, p0x, p0y, a01x, a01y, b01x, b01y, mx, my, depth - 1);
                    self(self, mx, my, b12x, b12y, a23x, a23y, p3x, p3y, depth - 1);
                };

                // ArcTo discretiser. Fixed 32 segments — adaptive step
                // by radius could refine later; for HATCH-scale arcs
                // (typically ≤ a few hundred pixels) 32 is smooth.
                auto arc_segments = [&](float cx, float cy, float r,
                                        float sa, float ea) {
                    constexpr int N = 32;
                    float sweep = ea - sa;
                    if (sweep <= 0.0f) sweep += 6.2831853f;
                    if (sweep > 6.2831853f) sweep = 6.2831853f;
                    for (int i = 1; i <= N; ++i) {
                        float t = sa + sweep
                                  * static_cast<float>(i)
                                  / static_cast<float>(N);
                        append(cx + r * std::cos(t),
                               cy + r * std::sin(t));
                    }
                };

                bool started = false;
                for (unsigned int i = 0; i < verb_count; ++i) {
                    unsigned int verb = 0;
                    if (!reader.read_u32(verb)) return false;
                    switch (static_cast<PathVerb>(verb)) {
                    case PathVerb::MoveTo: {
                        float x = 0, y = 0;
                        if (!reader.read_f32(x) || !reader.read_f32(y))
                            return false;
                        if (!started) {
                            append(x, y);
                            started = true;
                        }
                        // else: ignore subsequent MoveTos (single-loop
                        // fill only — multi-loop / hole semantics are
                        // out of scope for now).
                        break;
                    }
                    case PathVerb::LineTo: {
                        float x = 0, y = 0;
                        if (!reader.read_f32(x) || !reader.read_f32(y))
                            return false;
                        append(x, y);
                        if (!started) started = true;
                        break;
                    }
                    case PathVerb::QuadTo: {
                        float c1x = 0, c1y = 0, x = 0, y = 0;
                        if (!reader.read_f32(c1x) || !reader.read_f32(c1y)
                            || !reader.read_f32(x) || !reader.read_f32(y))
                            return false;
                        if (!polygon.empty()) {
                            float p0x = polygon[polygon.size() - 2];
                            float p0y = polygon[polygon.size() - 1];
                            flatten_quad(flatten_quad, p0x, p0y,
                                         c1x, c1y, x, y, max_depth);
                        }
                        break;
                    }
                    case PathVerb::CubicTo: {
                        float c1x = 0, c1y = 0, c2x = 0, c2y = 0;
                        float x = 0, y = 0;
                        if (!reader.read_f32(c1x) || !reader.read_f32(c1y)
                            || !reader.read_f32(c2x) || !reader.read_f32(c2y)
                            || !reader.read_f32(x) || !reader.read_f32(y))
                            return false;
                        if (!polygon.empty()) {
                            float p0x = polygon[polygon.size() - 2];
                            float p0y = polygon[polygon.size() - 1];
                            flatten_cubic(flatten_cubic, p0x, p0y,
                                          c1x, c1y, c2x, c2y,
                                          x, y, max_depth);
                        }
                        break;
                    }
                    case PathVerb::ArcTo: {
                        float acx = 0, acy = 0, ar = 0;
                        float asa = 0, aea = 0;
                        if (!reader.read_f32(acx) || !reader.read_f32(acy)
                            || !reader.read_f32(ar)
                            || !reader.read_f32(asa) || !reader.read_f32(aea))
                            return false;
                        if (ar > 0.0f) arc_segments(acx, acy, ar, asa, aea);
                        break;
                    }
                    case PathVerb::Close:
                        // Close is implicit on a fill — the polygon
                        // wraps to its first vertex automatically.
                        break;
                    default:
                        return false;
                    }
                }

                if (polygon.size() < 6) break;  // < 3 vertices
                auto& tris = scratch.fill_tris;
                tris.clear();
                polygon_ear_clip(polygon, tris, scratch.fill_ear_remain);
                // Append vertices via push_back so the vector's own
                // amortised doubling growth applies. A naive
                // dst.reserve(dst.size() + small_delta) here is a
                // performance trap: libc++'s reserve allocates
                // EXACTLY the requested capacity, so calling it once
                // per HATCH with a small delta forces a realloc-and-
                // copy on every fill — O(N²) cumulative on dense
                // CAD content (36 095 fills × growing buffer ≈ 2 s).
                ensure_triangle_order_batch(scratch);
                auto& dst = scratch.batches.back().tri_vertices;
                for (std::size_t t = 0; t + 5 < tris.size(); t += 6) {
                    // Push three vertices for this triangle. Hardware
                    // rasterisation handles edge coverage exactly so
                    // adjacent triangles tile pixel-perfectly without
                    // the sub-pixel gaps the previous CPU scanline
                    // path produced for slim hatch slivers.
                    TriVertexGPU v;
                    v.color[0] = cr; v.color[1] = cg;
                    v.color[2] = cb; v.color[3] = ca;
                    v.pos[0] = tris[t];     v.pos[1] = tris[t + 1]; dst.push_back(v);
                    v.pos[0] = tris[t + 2]; v.pos[1] = tris[t + 3]; dst.push_back(v);
                    v.pos[0] = tris[t + 4]; v.pos[1] = tris[t + 5]; dst.push_back(v);
                }
                break;
            }
            case Cmd::FillQuads: {
                unsigned int count = 0;
                if (!reader.read_u32(count))
                    return false;
                ensure_triangle_order_batch(scratch);
                auto& dst = scratch.batches.back().tri_vertices;
                dst.reserve(dst.size() + static_cast<std::size_t>(count) * 6);
                for (unsigned int i = 0; i < count; ++i) {
                    unsigned int packed = 0;
                    float x0 = 0.0f, y0 = 0.0f;
                    float x1 = 0.0f, y1 = 0.0f;
                    float x2 = 0.0f, y2 = 0.0f;
                    float x3 = 0.0f, y3 = 0.0f;
                    if (!reader.read_u32(packed)
                        || !reader.read_f32(x0) || !reader.read_f32(y0)
                        || !reader.read_f32(x1) || !reader.read_f32(y1)
                        || !reader.read_f32(x2) || !reader.read_f32(y2)
                        || !reader.read_f32(x3) || !reader.read_f32(y3))
                        return false;
                    auto color = unpack_color(packed);
                    TriVertexGPU v;
                    v.color[0] = color.r / 255.0f;
                    v.color[1] = color.g / 255.0f;
                    v.color[2] = color.b / 255.0f;
                    v.color[3] = color.a / 255.0f;
                    v.pos[0] = x0; v.pos[1] = y0; dst.push_back(v);
                    v.pos[0] = x1; v.pos[1] = y1; dst.push_back(v);
                    v.pos[0] = x2; v.pos[1] = y2; dst.push_back(v);
                    v.pos[0] = x0; v.pos[1] = y0; dst.push_back(v);
                    v.pos[0] = x2; v.pos[1] = y2; dst.push_back(v);
                    v.pos[0] = x3; v.pos[1] = y3; dst.push_back(v);
                }
                break;
            }
            case Cmd::FillRects: {
                unsigned int count = 0;
                if (!reader.read_u32(count))
                    return false;
                for (unsigned int i = 0; i < count; ++i) {
                    float x = 0.0f, y = 0.0f;
                    float w = 0.0f, h = 0.0f;
                    unsigned int packed = 0;
                    if (!reader.read_f32(x) || !reader.read_f32(y)
                        || !reader.read_f32(w) || !reader.read_f32(h)
                        || !reader.read_u32(packed))
                        return false;
                    auto color = unpack_color(packed);
                    append_color_instance(
                        scratch.batches.back().colors,
                        x, y, w, h,
                        color.r / 255.0f, color.g / 255.0f,
                        color.b / 255.0f, color.a / 255.0f);
                }
                break;
            }
            case Cmd::LinearGradientRect: {
                float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
                unsigned int from_packed = 0;
                unsigned int to_packed = 0;
                unsigned int axis_raw = 0;
                unsigned int steps = 0;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(w) || !reader.read_f32(h)
                    || !reader.read_u32(from_packed)
                    || !reader.read_u32(to_packed)
                    || !reader.read_u32(axis_raw)
                    || !reader.read_u32(steps))
                    return false;
                append_linear_gradient_instances(
                    scratch.batches.back().colors,
                    x,
                    y,
                    w,
                    h,
                    unpack_color(from_packed),
                    unpack_color(to_packed),
                    gradient_axis_from_wire(axis_raw),
                    steps);
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

inline constexpr char MSL_SHADERS[] = R"(
#include <metal_stdlib>
using namespace metal;

struct Uniforms {
    float2 viewport;
    float content_scale;
    float _pad;
};

// Triangle pipeline — raw 3-vertex triangle list with per-vertex
// colour. Used by Cmd::FillPath after CPU ear-clipping. Hardware
// rasterisation gives pixel-perfect coverage on shared edges, fixing
// the sub-pixel gaps the previous CPU scanline path produced for slim
// HATCH slivers (e.g. colorwh.dwg's inner-radius wedge tips).
//
// `packed_*` types disable MSL's default 16-byte float4 alignment so
// the shader-side struct matches C++ TriVertexGPU's 24-byte layout
// (8 bytes pos + 16 bytes colour, no inter-field padding). With
// non-packed float4, Metal would insert 8 bytes of pad between pos
// and colour and `verts[vi]` would stride by 32 bytes, reading
// colour from the WRONG offset on every vertex — every triangle
// would be drawn with arbitrary colour bits and the result would
// look like the old broken render with extra glitches.
struct TriVertex {
    packed_float2 pos;
    packed_float4 color;
};

struct TriVsOut {
    float4 pos [[position]];
    float4 color;
};

vertex TriVsOut vs_tri(
    uint vi [[vertex_id]],
    constant Uniforms& u [[buffer(0)]],
    const device TriVertex* verts [[buffer(1)]]
) {
    float2 p = float2(verts[vi].pos);
    float4 c = float4(verts[vi].color);
    float cx = (p.x / u.viewport.x) * 2.0 - 1.0;
    float cy = 1.0 - (p.y / u.viewport.y) * 2.0;
    TriVsOut out;
    out.pos = float4(cx, cy, 0, 1);
    out.color = c;
    return out;
}

fragment float4 fs_tri(TriVsOut in [[stage_in]]) {
    return in.color;
}

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
    if (draw_type == 5u && radius > 0.0) {
        float soft_edge = max(in.params.w, 0.5);
        float2 half_size = in.rect_size * 0.5;
        float2 p = abs(in.local_pos - half_size) - half_size + float2(radius);
        float d = length(max(p, float2(0.0))) - radius;
        if (d > 0.5) discard_fragment();
        float outer = clamp(0.5 - d, 0.0, 1.0);
        float feather = clamp((-d) / soft_edge, 0.0, 1.0);
        float alpha = in.color.a * min(outer, feather);
        return float4(in.color.rgb * alpha, alpha);
    }
    if (draw_type == 3u && radius > 0.0) {
        float2 half_size = in.rect_size * 0.5;
        float2 p = abs(in.local_pos - half_size) - half_size + float2(radius);
        float d = length(max(p, float2(0.0))) - radius;
        if (d > 0.5 || d < -border_w - 0.5) discard_fragment();
        float outer = clamp(0.5 - d, 0.0, 1.0);
        float inner = clamp(d + border_w + 0.5, 0.0, 1.0);
        float alpha = in.color.a * min(outer, inner);
        return float4(in.color.rgb * alpha, alpha);
    }
    if (draw_type == 1u) {
        float2 lp = in.local_pos;
        float2 sz = in.rect_size;
        if (lp.x > border_w && lp.x < sz.x - border_w &&
            lp.y > border_w && lp.y < sz.y - border_w) discard_fragment();
        return in.color;
    }
    if (draw_type == 3u) {
        float2 lp = in.local_pos;
        float2 sz = in.rect_size;
        if (lp.x > border_w && lp.x < sz.x - border_w &&
            lp.y > border_w && lp.y < sz.y - border_w) discard_fragment();
        float alpha = in.color.a;
        return float4(in.color.rgb * alpha, alpha);
    }
    return in.color;
}

struct MaterialVsOut {
    float4 pos [[position]];
    float2 local_pos;
    float2 rect_size;
    float2 screen_pos;
    float2 screen_uv;
    float content_scale;
    float4 tint;
    float4 params;
    float4 optics;
    float4 effects;
    float4 sampling;
    float4 luminance_curve;
    float4 interaction;
    float4 interaction_lens;
    float4 control_morph;
    float4 transition_optics;
    float4 refraction;
    float4 edge_optics;
    float4 spectral_tint;
    float4 lighting;
    float4 thickness;
    float4 dispersion;
    float4 scroll_edge;
    float4 prominent_glass;
    float4 clear_glass;
    float4 group_rect;
    float4 group_effects;
    float4 fusion_optics;
    float4 bridge_motion;
    float4 bridge_optics;
};

struct MaterialInstance {
    float4 rect;
    float4 tint;
    float4 params;
    float4 optics;
    float4 effects;
    float4 sampling;
    float4 luminance_curve;
    float4 interaction;
    float4 interaction_lens;
    float4 control_morph;
    float4 transition_optics;
    float4 refraction;
    float4 edge_optics;
    float4 spectral_tint;
    float4 lighting;
    float4 thickness;
    float4 dispersion;
    float4 scroll_edge;
    float4 prominent_glass;
    float4 clear_glass;
    float4 group_rect;
    float4 group_effects;
    float4 fusion_optics;
    float4 bridge_motion;
    float4 bridge_optics;
};

vertex MaterialVsOut vs_material(
    uint vi [[vertex_id]],
    uint ii [[instance_id]],
    constant Uniforms& u [[buffer(0)]],
    const device MaterialInstance* instances [[buffer(1)]]
) {
    constexpr float2 corners[] = {
        float2(0,0), float2(1,0), float2(0,1),
        float2(1,0), float2(1,1), float2(0,1),
    };
    float2 c = corners[vi];
    MaterialInstance inst = instances[ii];
    float control_morph_scale_delta = clamp(
        inst.control_morph.x,
        -0.06,
        0.06);
    float2 material_size = max(
        inst.rect.zw * (1.0 + control_morph_scale_delta),
        float2(1.0));
    float2 material_origin =
        inst.rect.xy + (inst.rect.zw - material_size) * 0.5;
    float px = material_origin.x + c.x * material_size.x;
    float py = material_origin.y + c.y * material_size.y;
    float cx = (px / u.viewport.x) * 2.0 - 1.0;
    float cy = 1.0 - (py / u.viewport.y) * 2.0;
    MaterialVsOut out;
    out.pos = float4(cx, cy, 0, 1);
    out.local_pos = c * material_size;
    out.rect_size = material_size;
    out.screen_pos = float2(px, py);
    out.screen_uv = float2(px / u.viewport.x, py / u.viewport.y);
    out.content_scale = u.content_scale;
    out.tint = inst.tint;
    out.params = inst.params;
    out.params.x = min(
        max(out.params.x * (1.0 + control_morph_scale_delta), 0.0),
        min(material_size.x, material_size.y) * 0.5);
    out.optics = inst.optics;
    out.effects = inst.effects;
    out.sampling = inst.sampling;
    out.luminance_curve = inst.luminance_curve;
    out.interaction = inst.interaction;
    out.interaction_lens = inst.interaction_lens;
    out.control_morph = inst.control_morph;
    out.transition_optics = inst.transition_optics;
    out.refraction = inst.refraction;
    out.edge_optics = inst.edge_optics;
    out.spectral_tint = inst.spectral_tint;
    out.lighting = inst.lighting;
    out.thickness = inst.thickness;
    out.dispersion = inst.dispersion;
    out.scroll_edge = inst.scroll_edge;
    out.prominent_glass = inst.prominent_glass;
    out.clear_glass = inst.clear_glass;
    out.group_rect = inst.group_rect;
    out.group_effects = inst.group_effects;
    out.fusion_optics = inst.fusion_optics;
    out.bridge_motion = inst.bridge_motion;
    out.bridge_optics = inst.bridge_optics;
    return out;
}

fragment float4 fs_material(
    MaterialVsOut in [[stage_in]],
    texture2d<float> backdrop [[texture(0)]],
    sampler samp [[sampler(0)]]
) {
    float radius = in.params.x;
    float edge_width = max(in.effects.x, 0.5);
    float2 rect_edge_distance = min(
        in.local_pos,
        max(in.rect_size - in.local_pos, float2(0.0)));
    float signed_edge_distance = max(
        min(rect_edge_distance.x, rect_edge_distance.y),
        0.0);
    float edge_alpha = 1.0;
    if (radius > 0.0) {
        float2 half_size = in.rect_size * 0.5;
        float2 p = abs(in.local_pos - half_size) - half_size + float2(radius);
        float d = length(max(p, float2(0.0))) - radius;
        if (d > 0.5) discard_fragment();
        edge_alpha = clamp(0.5 - d, 0.0, 1.0);
        signed_edge_distance = abs(d);
    }
    float2 normalized_local =
        (in.local_pos / max(in.rect_size, float2(1.0)) - float2(0.5)) * 2.0;
    float group_blend_strength = clamp(in.group_effects.x, 0.0, 1.0);
    float inner_edge_alpha_blend_strength =
        clamp(in.group_effects.y, 0.0, 1.0);
    float fusion_strength = clamp(in.fusion_optics.x, 0.0, 1.0);
    float fusion_lensing_gain = clamp(in.fusion_optics.y, 1.0, 1.35);
    float fusion_edge_lift = clamp(in.fusion_optics.z, 0.0, 0.16);
    float fusion_shadow_gain = clamp(in.fusion_optics.w, 1.0, 1.32);
    float overlap_response_strength = clamp(in.group_effects.w, 0.0, 1.0);
    float bridge_motion_strength = clamp(in.bridge_optics.x, 0.0, 1.0);
    float bridge_flow_offset_gain = clamp(in.bridge_optics.y, 0.0, 0.60);
    float bridge_ribbon_width = clamp(in.bridge_optics.z, 0.08, 0.32);
    float union_execution =
        floor(fmod(floor(in.group_effects.z / 2.0), 2.0));
    float bridge_caustic_gain =
        clamp(
            1.0
                + bridge_motion_strength
                    * (0.60
                       + 0.24 * union_execution * bridge_flow_offset_gain),
            1.0,
            1.95);
    float bridge_highlight_gain = clamp(in.bridge_optics.w, 0.0, 0.30);
    float materialize_wave_strength =
        clamp(in.transition_optics.x, 0.0, 1.0);
    float materialize_edge_lift =
        clamp(in.transition_optics.y, 0.0, 0.18);
    float materialize_lensing_gain =
        clamp(in.transition_optics.z, 1.0, 1.32);
    float materialize_rim_position =
        clamp(in.transition_optics.w, 0.0, 1.0);
    if (group_blend_strength > 0.0
        && in.group_rect.z > 0.0
        && in.group_rect.w > 0.0) {
        float2 group_size = max(in.group_rect.zw, float2(1.0));
        float2 group_local = clamp(
            in.screen_pos - in.group_rect.xy,
            float2(0.0),
            group_size);
        float2 group_edge_distance = min(
            group_local,
            max(group_size - group_local, float2(0.0)));
        float group_signed_edge_distance = max(
            min(group_edge_distance.x, group_edge_distance.y),
            0.0);
        float group_radius = min(
            max(radius, 0.0),
            min(group_size.x, group_size.y) * 0.5);
        if (group_radius > 0.0) {
            float2 group_half_size = group_size * 0.5;
            float2 group_p =
                abs(group_local - group_half_size)
                - group_half_size
                + float2(group_radius);
            float group_d = length(max(group_p, float2(0.0))) - group_radius;
            group_signed_edge_distance = abs(group_d);
            float group_edge_alpha = clamp(0.5 - group_d, 0.0, 1.0);
            edge_alpha = mix(
                edge_alpha,
                max(edge_alpha, group_edge_alpha),
                inner_edge_alpha_blend_strength);
        }
        signed_edge_distance = mix(
            signed_edge_distance,
            group_signed_edge_distance,
            group_blend_strength);
        float2 group_normalized_local =
            (group_local / group_size - float2(0.5)) * 2.0;
        normalized_local = mix(
            normalized_local,
            group_normalized_local,
            group_blend_strength);
    }
    float soft_edge_radius = max(in.sampling.z, 0.0);
    if (soft_edge_radius > 0.0) {
        float feather = clamp(
            signed_edge_distance / soft_edge_radius,
            0.0,
            1.0);
        edge_alpha = min(edge_alpha, feather);
    }

    float2 texel = 1.0 / float2(float(backdrop.get_width()),
                                float(backdrop.get_height()));
    float blur_points = clamp(in.params.y, 0.0, 36.0);
    uint sample_taps = uint(clamp(round(in.params.w), 1.0, 25.0));
    float blur_step_scale = max(in.sampling.x, 0.0);
    uint kernel_radius = uint(clamp(round(in.sampling.y), 0.0, 2.0));
    float content_scale = max(in.content_scale, 1.0);
    float2 step_uv =
        texel * max(1.0, blur_points * content_scale * blur_step_scale);
    float glass_thickness = clamp(in.thickness.x, 0.0, 1.0);
    float glass_lensing_gain = clamp(in.thickness.y, 1.0, 1.55);
    float glass_shadow_gain = clamp(in.thickness.z, 1.0, 1.55);
    float glass_scattering_gain = clamp(in.thickness.w, 1.0, 1.45);
    float refraction_strength = clamp(in.refraction.x, 0.0, 1.0);
    float refraction_edge_bias = clamp(in.refraction.y, 0.0, 1.0);
    float refraction_offset_pixels = clamp(in.refraction.z, 0.0, 8.0)
        * (refraction_strength > 0.0 ? 1.0 : 0.0)
        * glass_lensing_gain
        * fusion_lensing_gain
        * materialize_lensing_gain
        * (1.0 - 0.12 * overlap_response_strength);
    float refraction_edge_caustic =
        clamp(
            in.refraction.w
                * glass_lensing_gain
                * (1.0
                   + 0.34 * fusion_strength
                   + 0.24 * materialize_wave_strength),
            0.0,
            0.46);
    float edge_bevel_width = clamp(
        in.edge_optics.x * (1.0 + 0.16 * glass_thickness),
        0.0,
        18.0);
    float edge_inner_highlight =
        clamp(
            in.edge_optics.y + fusion_edge_lift + materialize_edge_lift,
            0.0,
            0.90);
    float edge_outer_shadow =
        clamp(
            in.edge_optics.z * glass_shadow_gain * fusion_shadow_gain
                + 0.035 * fusion_strength
                + 0.028 * materialize_wave_strength,
            0.0,
            0.80);
    float edge_chromatic_fringe = clamp(in.edge_optics.w, 0.0, 0.16);
    float spectral_warmth = clamp(in.spectral_tint.x, 0.0, 0.22);
    float spectral_coolness = clamp(in.spectral_tint.y, 0.0, 0.22);
    float spectral_dispersion = clamp(in.spectral_tint.z, 0.0, 0.22);
    float spectral_rim_tint = clamp(in.spectral_tint.w, 0.0, 0.28);
    float glass_dispersion_axial = clamp(in.dispersion.x, 0.0, 3.5);
    float glass_dispersion_tangential = clamp(in.dispersion.y, 0.0, 2.75);
    float glass_prismatic_gain = clamp(in.dispersion.z, 1.0, 1.80);
    float glass_caustic_spread = clamp(in.dispersion.w, 0.0, 0.44);
    float scroll_edge_extent = clamp(in.scroll_edge.x, 0.0, 96.0);
    float scroll_edge_dissolve = clamp(in.scroll_edge.y, 0.0, 0.60);
    float scroll_edge_dimming = clamp(in.scroll_edge.z, 0.0, 0.45);
    float scroll_edge_hard_style = clamp(in.scroll_edge.w, 0.0, 0.70);
    float prominent_intensity = clamp(in.prominent_glass.x, 0.0, 1.0);
    float prominent_tint_weight = clamp(in.prominent_glass.y, 0.0, 0.74);
    float prominent_edge_lift = clamp(in.prominent_glass.z, 0.0, 0.30);
    float prominent_lensing_gain = clamp(in.prominent_glass.w, 1.0, 1.36);
    float clear_glass_dimming = clamp(in.clear_glass.x, 0.0, 0.34);
    float clear_glass_contrast = clamp(in.clear_glass.y, 0.0, 0.28);
    float clear_glass_brightness = clamp(in.clear_glass.z, 0.0, 1.0);
    float clear_glass_detail = clamp(in.clear_glass.w, 0.0, 1.0);
    clear_glass_dimming = max(
        clear_glass_dimming,
        0.060 * overlap_response_strength);
    clear_glass_contrast = max(
        clear_glass_contrast,
        0.045 * overlap_response_strength);
    edge_inner_highlight = clamp(
        edge_inner_highlight + prominent_edge_lift,
        0.0,
        0.96);
    edge_outer_shadow = clamp(
        edge_outer_shadow + 0.040 * prominent_intensity,
        0.0,
        0.84);
    float control_morph_depth = clamp(in.control_morph.y, 0.0, 0.40);
    float control_morph_edge_lift = clamp(in.control_morph.z, 0.0, 0.35);
    float control_morph_shadow_compression =
        clamp(in.control_morph.w, 0.0, 0.35);
    float2 default_light_dir = normalize(float2(-0.58, -0.82));
    float2 dynamic_light_raw = in.lighting.xy;
    float2 dynamic_light_dir = length(dynamic_light_raw) > 0.0001
        ? normalize(dynamic_light_raw)
        : default_light_dir;
    float dynamic_light_highlight = clamp(in.lighting.z, 0.0, 0.45);
    float dynamic_light_shadow = clamp(in.lighting.w, 0.0, 0.36);
    float normalized_len = length(normalized_local);
    float2 refraction_dir = normalized_len > 0.0001
        ? normalized_local / normalized_len
        : float2(0.0);
    float edge_span = max(edge_width * (2.0 + 5.0 * refraction_edge_bias),
                          min(in.rect_size.x, in.rect_size.y) * 0.035);
    float edge_lens = 1.0 - smoothstep(0.0, edge_span, signed_edge_distance);
    float2 bridge_dir_raw = in.bridge_motion.xy;
    float bridge_dir_length = length(bridge_dir_raw);
    float2 bridge_dir = bridge_dir_length > 0.0001
        ? bridge_dir_raw / bridge_dir_length
        : float2(0.0);
    float2 bridge_tangent = float2(-bridge_dir.y, bridge_dir.x);
    float bridge_band = 0.0;
    float bridge_core = 0.0;
    float bridge_shear = 0.0;
    float bridge_axial = 0.0;
    float bridge_lateral_signed = 0.0;
    float bridge_lateral = 0.0;
    float bridge_width = bridge_ribbon_width;
    float bridge_length = 0.0;
    if (bridge_motion_strength > 0.0001 && bridge_dir_length > 0.0001) {
        float2 bridge_anchor =
            clamp(in.bridge_motion.zw, float2(0.0), float2(1.0))
            * max(in.rect_size, float2(1.0));
        float2 bridge_delta =
            (in.local_pos - bridge_anchor)
            / max(in.rect_size, float2(1.0));
        bridge_axial = abs(dot(bridge_delta, bridge_dir));
        bridge_lateral_signed = dot(bridge_delta, bridge_tangent);
        bridge_lateral = abs(bridge_lateral_signed);
        bridge_length = clamp(
            0.34
                + group_blend_strength * 0.32
                + union_execution * bridge_flow_offset_gain * 0.18,
            0.34,
            mix(0.72, 0.84, union_execution));
        bridge_band =
            (1.0 - smoothstep(bridge_width,
                              bridge_width + 0.18,
                              bridge_lateral))
            * (1.0 - smoothstep(bridge_length,
                                bridge_length + 0.20,
                                bridge_axial))
            * bridge_motion_strength;
        bridge_core =
            1.0 - smoothstep(
                bridge_width * 0.28,
                bridge_width + 0.18,
                bridge_lateral);
        bridge_shear =
            clamp(
                bridge_lateral_signed / max(bridge_width + 0.18, 0.001),
                -1.0,
                1.0)
            * bridge_core;
    }
    float2 refraction_uv =
        refraction_dir * texel
        * (refraction_offset_pixels * content_scale)
        * edge_lens;
    refraction_uv += bridge_dir
        * texel
        * (refraction_offset_pixels * content_scale)
        * bridge_band
        * bridge_flow_offset_gain;
    refraction_uv += bridge_tangent
        * texel
        * (refraction_offset_pixels * content_scale)
        * bridge_shear
        * bridge_band
        * bridge_flow_offset_gain
        * (0.22 + 0.30 * bridge_core)
        * (0.38 + 0.62 * union_execution);
    float pointer_lens_strength = clamp(in.interaction_lens.w, 0.0, 0.35);
    float2 pointer_anchor =
        clamp(in.interaction_lens.xy, float2(0.0), float2(1.0))
        * max(in.rect_size, float2(1.0));
    float pointer_lens_radius =
        clamp(in.interaction_lens.z, 0.05, 1.0)
        * max(min(in.rect_size.x, in.rect_size.y), 1.0);
    float2 pointer_delta = in.local_pos - pointer_anchor;
    float pointer_distance = length(pointer_delta);
    float2 pointer_dir = pointer_distance > 0.0001
        ? pointer_delta / pointer_distance
        : -refraction_dir;
    float pointer_lens_raw = 0.0;
    float pointer_lens = 0.0;
    refraction_uv *= prominent_lensing_gain;
    if (pointer_lens_strength > 0.0001) {
        pointer_lens_raw = 1.0 - smoothstep(
            0.0,
            pointer_lens_radius,
            pointer_distance);
        pointer_lens = pointer_lens_raw * pointer_lens_raw;
        refraction_uv += pointer_dir
            * texel
            * (refraction_offset_pixels * content_scale)
            * pointer_lens
            * pointer_lens_strength
            * 1.65
            * prominent_lensing_gain;
    }
    float surface_tension_strength = clamp(
        0.014 * glass_thickness
            + 0.018 * (glass_lensing_gain - 1.0)
            + 0.020 * glass_caustic_spread
            + 0.014 * fusion_strength
            + 0.018 * materialize_wave_strength
            + 0.012 * control_morph_depth
            + 0.022 * pointer_lens_strength * pointer_lens_raw
            + 0.014 * bridge_band,
        0.0,
        0.090);
    if (surface_tension_strength > 0.0001) {
        float2 surface_tangent = float2(-refraction_dir.y, refraction_dir.x);
        float2 tension_raw_dir =
            refraction_dir * (0.50 + 0.50 * edge_lens)
            - dynamic_light_dir * 0.34
            + bridge_dir * bridge_band * 0.56
            + pointer_dir * pointer_lens * pointer_lens_strength * 0.82;
        float tension_dir_len = length(tension_raw_dir);
        float2 tension_dir = tension_dir_len > 0.0001
            ? tension_raw_dir / tension_dir_len
            : -dynamic_light_dir;
        float2 tension_cross = float2(-tension_dir.y, tension_dir.x);
        float tension_surface =
            1.0 - smoothstep(0.16, 1.18, normalized_len);
        float tension_rim = edge_lens
            * (0.36
               + 0.64
                   * (1.0 - smoothstep(
                       0.0,
                       max(edge_width * 1.30, 0.5),
                       signed_edge_distance)));
        float tension_contact =
            pointer_lens_raw * pointer_lens_strength
            + bridge_band * (0.55 + 0.45 * bridge_core);
        float tension_gate = clamp(
            tension_surface * 0.34
                + tension_rim * 0.46
                + tension_contact * 0.30,
            0.0,
            1.0);
        float tension_wave = sin(
            dot(normalized_local, tension_dir) * 7.5
                + dot(normalized_local, surface_tangent) * 3.0
                + normalized_len * 5.0
                + glass_caustic_spread * 19.0);
        float tension_fold = sin(
            dot(normalized_local, tension_cross) * 5.5
                - normalized_len * 4.0
                + glass_thickness * 13.0);
        float tension_span =
            (1.4
             + 3.4 * glass_thickness
             + 3.0 * glass_caustic_spread
             + 0.10 * blur_points)
            * content_scale
            * (0.70 + 0.30 * prominent_lensing_gain);
        refraction_uv += tension_dir
            * texel
            * tension_span
            * surface_tension_strength
            * tension_gate
            * tension_wave;
        refraction_uv += tension_cross
            * texel
            * tension_span
            * surface_tension_strength
            * tension_gate
            * tension_fold
            * (0.46 + 0.54 * edge_lens);
    }
    float caustic_weight = clamp(
        edge_lens
            * refraction_edge_caustic
            * glass_prismatic_gain
            * (1.0 + 0.22 * prominent_intensity)
            * (1.0 + bridge_band * (bridge_caustic_gain - 1.0)),
        0.0,
        0.36);
    float2 dispersion_tangent = float2(-refraction_dir.y, refraction_dir.x);
    float dispersion_alignment = abs(dot(dispersion_tangent,
                                         dynamic_light_dir));
    float2 dispersion_uv =
        (refraction_dir * glass_dispersion_axial
            + dispersion_tangent
                * glass_dispersion_tangential
                * (0.48 + 0.52 * dispersion_alignment))
        * texel
        * content_scale
        * edge_lens;
    float fringe_weight = clamp(
        caustic_weight * (0.45 + 0.55 * refraction_edge_bias)
            + edge_lens
                * (edge_chromatic_fringe
                   + spectral_dispersion
                   + 0.12 * (glass_prismatic_gain - 1.0)
                   + glass_caustic_spread
                   + bridge_band * 0.030 * (bridge_caustic_gain - 1.0)),
        0.0,
        0.34);
    float2 fringe_uv =
        refraction_uv * (0.55 + 0.65 * refraction_edge_bias)
        + refraction_dir
            * texel
            * content_scale
            * edge_lens
            * edge_chromatic_fringe
            * 2.0
        + dispersion_uv;
    float4 acc = float4(0.0);
    float weight_sum = 0.0;
    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            int ax = abs(x);
            int ay = abs(y);
            int manhattan = ax + ay;
            int chebyshev = max(ax, ay);
            bool within_radius = uint(chebyshev) <= kernel_radius;
            bool include = within_radius && (sample_taps >= 25u
                || (sample_taps >= 13u && manhattan <= 2)
                || (sample_taps >= 9u && chebyshev <= 1)
                || (sample_taps >= 5u && manhattan <= 1)
                || (sample_taps >= 1u && manhattan == 0));
            if (!include)
                continue;
            float axis_weight_x = (ax == 0) ? 7.0 : ((ax == 1) ? 4.0 : 1.0);
            float axis_weight_y = (ay == 0) ? 7.0 : ((ay == 1) ? 4.0 : 1.0);
            float weight = axis_weight_x * axis_weight_y;
            float2 uv = clamp(
                in.screen_uv
                    + refraction_uv
                    + float2(float(x), float(y)) * step_uv,
                float2(0.0),
                float2(1.0));
            acc += backdrop.sample(samp, uv) * weight;
            weight_sum += weight;
        }
    }
    float4 blurred = acc / weight_sum;
    float3 refracted_rgb = blurred.rgb;
    if (fringe_weight > 0.0001) {
        float2 warm_uv = clamp(
            in.screen_uv + refraction_uv + fringe_uv,
            float2(0.0),
            float2(1.0));
        float2 cool_uv = clamp(
            in.screen_uv + refraction_uv - fringe_uv,
            float2(0.0),
            float2(1.0));
        float2 prism_uv = clamp(
            in.screen_uv
                + refraction_uv
                + dispersion_tangent
                    * texel
                    * content_scale
                    * glass_dispersion_tangential
                    * edge_lens,
            float2(0.0),
            float2(1.0));
        float4 warm_sample = backdrop.sample(samp, warm_uv);
        float4 cool_sample = backdrop.sample(samp, cool_uv);
        float4 prism_sample = backdrop.sample(samp, prism_uv);
        refracted_rgb.r = mix(
            refracted_rgb.r,
            clamp(warm_sample.r + spectral_warmth, 0.0, 1.0),
            fringe_weight);
        refracted_rgb.g = mix(
            refracted_rgb.g,
            clamp((warm_sample.g + cool_sample.g + prism_sample.g) / 3.0
                    + 0.22 * (spectral_warmth + spectral_coolness),
                  0.0,
                  1.0),
            fringe_weight
                * clamp(
                    spectral_rim_tint
                        + 0.36 * (glass_prismatic_gain - 1.0)
                        + 0.20 * glass_caustic_spread,
                    0.0,
                    0.70));
        refracted_rgb.b = mix(
            refracted_rgb.b,
            clamp(cool_sample.b + spectral_coolness, 0.0, 1.0),
            fringe_weight);
    }
    float luma = dot(refracted_rgb, float3(0.2126, 0.7152, 0.0722));
    float saturation = max(in.optics.x, 0.0);
    float luminance_floor = clamp(in.optics.y, 0.0, 1.0);
    float luminance_gain = max(in.optics.z, 0.0);
    float3 backdrop_rgb = mix(float3(luma), refracted_rgb, saturation);
    float gamma = clamp(in.luminance_curve.x, 0.25, 4.0);
    float midpoint = clamp(in.luminance_curve.y, 0.0, 1.0);
    float contrast = clamp(in.luminance_curve.z, 0.0, 4.0);
    backdrop_rgb = pow(clamp(backdrop_rgb, 0.0, 1.0), float3(gamma));
    backdrop_rgb = clamp((backdrop_rgb - midpoint) * contrast + midpoint,
                         0.0,
                         1.0);
    backdrop_rgb = clamp(max(backdrop_rgb * luminance_gain,
                            float3(luminance_floor)), 0.0, 1.0);
    if (clear_glass_dimming > 0.0001
        || clear_glass_contrast > 0.0001) {
        float clear_luma =
            dot(backdrop_rgb, float3(0.2126, 0.7152, 0.0722));
        float brightness_gate =
            max(smoothstep(0.52, 0.92, clear_luma),
                clear_glass_brightness);
        float detail_gate = 0.35 + 0.65 * clear_glass_detail;
        float dimming_weight =
            clear_glass_dimming
            * (0.42 + 0.58 * brightness_gate)
            * detail_gate;
        float3 dimmed = backdrop_rgb * (1.0 - dimming_weight);
        float3 contrasted = clamp(
            (dimmed - float3(0.50))
                * (1.0 + clear_glass_contrast * detail_gate)
                + float3(0.50),
            0.0,
            1.0);
        backdrop_rgb = mix(dimmed, contrasted, 0.72);
    }
    if (overlap_response_strength > 0.0001) {
        float overlap_center =
            1.0 - smoothstep(0.18, 1.10, normalized_len);
        float overlap_luma =
            dot(backdrop_rgb, float3(0.2126, 0.7152, 0.0722));
        float3 overlap_settled =
            mix(backdrop_rgb, float3(overlap_luma), 0.42);
        overlap_settled *=
            1.0 - 0.035 * overlap_response_strength * overlap_center;
        backdrop_rgb = mix(
            backdrop_rgb,
            overlap_settled,
            0.10 + 0.18 * overlap_response_strength);
    }
    if (union_execution > 0.5 && bridge_band > 0.0001) {
        float bridge_settle_strength = clamp(
            bridge_band
                * bridge_motion_strength
                * (0.44 + 0.56 * bridge_flow_offset_gain),
            0.0,
            1.0);
        float bridge_luma =
            dot(backdrop_rgb, float3(0.2126, 0.7152, 0.0722));
        float3 bridge_settled =
            mix(
                backdrop_rgb,
                float3(bridge_luma),
                0.30 + 0.18 * bridge_flow_offset_gain);
        bridge_settled = clamp(
            (bridge_settled - float3(0.50))
                    * (1.0 + 0.10 * bridge_flow_offset_gain)
                + float3(0.50),
            0.0,
            1.0);
        bridge_settled *=
            1.0 - 0.030 * bridge_settle_strength;
        backdrop_rgb = mix(
            backdrop_rgb,
            bridge_settled,
            clamp(
                0.10 * bridge_band + 0.24 * bridge_settle_strength,
                0.0,
                0.38));
    }
    float scattering_strength = clamp(
        (glass_scattering_gain - 1.0) * 0.22 + glass_thickness * 0.035,
        0.0,
        0.18);
    backdrop_rgb = mix(
        backdrop_rgb,
        mix(backdrop_rgb, float3(luma), 0.58),
        scattering_strength * (0.55 + 0.45 * edge_lens));
    if (scroll_edge_extent > 0.0001
        && (scroll_edge_dissolve > 0.0001
            || scroll_edge_dimming > 0.0001
            || scroll_edge_hard_style > 0.0001)) {
        float pocket_left = in.local_pos.x;
        float pocket_right = max(in.rect_size.x - in.local_pos.x, 0.0);
        float pocket_top = in.local_pos.y;
        float pocket_bottom = max(in.rect_size.y - in.local_pos.y, 0.0);
        float pocket_nearest = min(
            min(pocket_left, pocket_right),
            min(pocket_top, pocket_bottom));
        float2 pocket_dir = float2(-1.0, 0.0);
        if (pocket_right <= pocket_nearest) {
            pocket_dir = float2(1.0, 0.0);
        }
        if (pocket_top <= pocket_nearest) {
            pocket_dir = float2(0.0, -1.0);
        }
        if (pocket_bottom <= pocket_nearest) {
            pocket_dir = float2(0.0, 1.0);
        }
        float2 pocket_tangent = float2(-pocket_dir.y, pocket_dir.x);
        float pocket_band = 1.0 - smoothstep(
            0.0,
            max(scroll_edge_extent, 0.5),
            signed_edge_distance);
        float pocket_hard = scroll_edge_hard_style
            * (1.0 - smoothstep(0.34, 1.10, normalized_len));
        float pocket_gate = clamp(
            max(pocket_band, pocket_hard)
                * (0.42
                   + 0.50 * scroll_edge_dissolve
                   + 0.34 * scroll_edge_dimming
                   + 0.26 * scroll_edge_hard_style),
            0.0,
            1.0);
        float scroll_pocket_strength = clamp(
            pocket_gate
                * (0.035
                   + 0.030 * glass_thickness
                   + 0.030 * (glass_lensing_gain - 1.0)
                   + 0.024 * glass_caustic_spread
                   + 0.020 * fusion_strength
                   + 0.014 * bridge_band),
            0.0,
            0.18);
        if (scroll_pocket_strength > 0.0001) {
            float pocket_phase =
                dot(normalized_local, pocket_tangent)
                    * (7.0 + 4.0 * glass_caustic_spread)
                + signed_edge_distance
                    / max(scroll_edge_extent, 0.5)
                    * 5.0
                + scroll_edge_dissolve * 6.0
                + bridge_band * 4.0;
            float pocket_wave = 0.5 + 0.5 * sin(pocket_phase);
            float pocket_span =
                (2.0
                 + 0.12 * scroll_edge_extent
                 + 3.4 * glass_thickness
                 + 2.6 * glass_caustic_spread
                 + 0.10 * blur_points)
                * content_scale
                * (0.78 + 0.22 * glass_lensing_gain);
            float pocket_cross_span =
                (1.0
                 + 2.2 * glass_thickness
                 + 1.6 * glass_dispersion_tangential
                 + 3.0 * spectral_dispersion)
                * content_scale;
            float2 pocket_mirror_uv = clamp(
                in.screen_uv
                    - pocket_dir
                        * texel
                        * pocket_span
                        * (0.68 + 0.24 * pocket_band),
                float2(0.0),
                float2(1.0));
            float2 pocket_pull_uv = clamp(
                in.screen_uv
                    + pocket_dir
                        * texel
                        * pocket_span
                        * (0.36 + 0.24 * pocket_wave),
                float2(0.0),
                float2(1.0));
            float2 pocket_shear_uv = clamp(
                in.screen_uv
                    + pocket_tangent
                        * texel
                        * pocket_cross_span
                        * (0.58
                           + 0.22 * abs(bridge_shear)
                           + 0.16 * pocket_wave)
                    + dispersion_tangent
                        * texel
                        * pocket_cross_span
                        * 0.22,
                float2(0.0),
                float2(1.0));
            float3 pocket_mirror =
                backdrop.sample(samp, pocket_mirror_uv).rgb;
            float3 pocket_pull =
                backdrop.sample(samp, pocket_pull_uv).rgb;
            float3 pocket_shear =
                backdrop.sample(samp, pocket_shear_uv).rgb;
            float3 pocket_rgb =
                pocket_mirror * 0.42
                + pocket_pull * 0.34
                + pocket_shear * 0.24;
            float pocket_luma =
                dot(pocket_rgb, float3(0.2126, 0.7152, 0.0722));
            float pocket_surface_luma =
                dot(backdrop_rgb, float3(0.2126, 0.7152, 0.0722));
            float pocket_bright = smoothstep(
                pocket_surface_luma - 0.08,
                pocket_surface_luma + 0.30,
                pocket_luma);
            float pocket_dark = smoothstep(
                0.10,
                0.34,
                pocket_surface_luma - pocket_luma);
            float pocket_detail = smoothstep(
                0.03,
                0.30,
                length(pocket_mirror - pocket_pull) * 0.38
                    + length(pocket_shear - pocket_rgb) * 0.24);
            float3 pocket_tint = mix(
                pocket_rgb,
                float3(pocket_luma),
                pocket_dark * (0.18 + 0.18 * glass_shadow_gain));
            pocket_tint = mix(
                pocket_tint,
                pocket_tint * (float3(1.0) + 0.12 * in.tint.rgb),
                clamp(in.tint.a, 0.0, 1.0)
                    * (0.22 + 0.24 * prominent_intensity));
            pocket_tint += float3(
                spectral_warmth,
                0.10 * (spectral_warmth + spectral_coolness),
                spectral_coolness)
                * pocket_band
                * (0.08 + 0.24 * spectral_rim_tint);
            float pocket_weight = scroll_pocket_strength
                * (0.46
                   + 0.20 * pocket_detail
                   + 0.20 * pocket_bright
                   + 0.14 * (1.0 - scroll_edge_hard_style));
            backdrop_rgb = mix(
                backdrop_rgb,
                mix(backdrop_rgb, pocket_tint, 0.22 + 0.16 * pocket_detail),
                pocket_weight);
            backdrop_rgb += pocket_tint
                * pocket_weight
                * (0.010
                   + 0.020 * dynamic_light_highlight
                   + 0.014 * glass_prismatic_gain);
            float pocket_shadow = pocket_dark
                * pocket_weight
                * (0.020 + 0.036 * glass_shadow_gain)
                * (0.70 + 0.30 * scroll_edge_hard_style);
            backdrop_rgb *= 1.0 - clamp(pocket_shadow, 0.0, 0.065);
            backdrop_rgb = clamp(backdrop_rgb, 0.0, 1.0);
        }
    }
    if (scroll_edge_extent > 0.0001
        && (scroll_edge_dissolve > 0.0001
            || scroll_edge_dimming > 0.0001
            || scroll_edge_hard_style > 0.0001)) {
        float scroll_edge_band = 1.0 - smoothstep(
            0.0,
            max(scroll_edge_extent, 0.5),
            signed_edge_distance);
        float hard_plate = scroll_edge_hard_style
            * (1.0 - smoothstep(0.34, 1.10, normalized_len));
        float separation = clamp(max(scroll_edge_band, hard_plate), 0.0, 1.0);
        float scroll_luma = dot(backdrop_rgb, float3(0.2126, 0.7152, 0.0722));
        float3 dissolved = mix(backdrop_rgb, float3(scroll_luma), 0.72);
        float3 dimmed = dissolved * (1.0 - scroll_edge_dimming * separation);
        backdrop_rgb = mix(
            backdrop_rgb,
            dimmed,
            separation
                * clamp(scroll_edge_dissolve
                        + 0.32 * scroll_edge_hard_style,
                        0.0,
                        0.72));
    }
    if (control_morph_depth > 0.0001
        || control_morph_edge_lift > 0.0001
        || control_morph_shadow_compression > 0.0001) {
        float control_center =
            1.0 - smoothstep(0.12, 1.08, normalized_len);
        float control_edge = 1.0 - smoothstep(
            0.0,
            edge_width,
            signed_edge_distance);
        float control_rim =
            edge_lens * (0.45 + 0.55 * control_edge);
        float3 compressed = backdrop_rgb
            * (1.0
               - 0.22
                   * control_morph_shadow_compression
                   * control_center);
        backdrop_rgb = mix(backdrop_rgb, compressed, control_morph_depth);
        backdrop_rgb += float3(1.0)
            * control_rim
            * control_morph_edge_lift
            * 0.18;
        backdrop_rgb = clamp(backdrop_rgb, 0.0, 1.0);
    }
    if (fusion_strength > 0.0001) {
        float fusion_center =
            1.0 - smoothstep(0.18, 1.18, normalized_len);
        float fusion_rim =
            edge_lens
            * (0.52
               + 0.48
                   * (1.0 - smoothstep(
                       0.0,
                       max(edge_width * 1.6, 0.5),
                       signed_edge_distance)));
        backdrop_rgb = mix(
            backdrop_rgb,
            backdrop_rgb * (1.0 - 0.055 * fusion_strength * fusion_center),
            0.55 * fusion_strength);
        backdrop_rgb += float3(1.0)
            * fusion_rim
            * fusion_edge_lift
            * (0.42 + 0.58 * group_blend_strength);
        backdrop_rgb = clamp(backdrop_rgb, 0.0, 1.0);
    }
    if (materialize_wave_strength > 0.0001) {
        float materialize_band = 1.0 - smoothstep(
            0.0,
            0.24,
            abs(normalized_len - materialize_rim_position));
        float materialize_edge = edge_lens
            * (0.45
               + 0.55
                   * (1.0 - smoothstep(
                       0.0,
                       max(edge_width * 1.8, 0.5),
                       signed_edge_distance)));
        float materialize_rim =
            max(materialize_band * 0.72, materialize_edge * 0.42)
            * materialize_wave_strength;
        float materialize_luma =
            dot(backdrop_rgb, float3(0.2126, 0.7152, 0.0722));
        backdrop_rgb = mix(
            backdrop_rgb,
            mix(backdrop_rgb, float3(materialize_luma), 0.46),
            0.10 * materialize_wave_strength);
        backdrop_rgb += float3(1.0)
            * materialize_rim
            * materialize_edge_lift
            * 0.68;
        backdrop_rgb += float3(spectral_warmth, 0.0, spectral_coolness)
            * materialize_rim
            * 0.055;
        backdrop_rgb = clamp(backdrop_rgb, 0.0, 1.0);
    }
    float tint_strength = clamp(in.tint.a, 0.0, 1.0);
    float opacity = clamp(in.params.z, 0.0, 1.0);
    float tint_chroma = max(
        max(abs(in.tint.r - in.tint.g), abs(in.tint.g - in.tint.b)),
        abs(in.tint.b - in.tint.r));
    tint_chroma *= smoothstep(0.10, 0.52, tint_strength);
    float3 tint_chromatic_rgb = in.tint.rgb * tint_chroma;
    float3 rgb = mix(backdrop_rgb, in.tint.rgb, tint_strength);
    float edge = 1.0 - smoothstep(0.0, edge_width, signed_edge_distance);
    float edge_lift = clamp(in.luminance_curve.w, 0.0, 1.0);
    float environment_reflection_strength = clamp(
        0.030
            + 0.050 * glass_thickness
            + 0.038 * glass_caustic_spread
            + 0.032 * (glass_lensing_gain - 1.0)
            + 0.018 * prominent_intensity,
        0.0,
        0.18);
    if (environment_reflection_strength > 0.0001) {
        float2 reflection_raw_dir =
            -dynamic_light_dir + refraction_dir * (0.34 + 0.36 * edge_lens);
        float reflection_raw_len = length(reflection_raw_dir);
        float2 reflection_dir = reflection_raw_len > 0.0001
            ? reflection_raw_dir / reflection_raw_len
            : -dynamic_light_dir;
        float2 reflection_tangent = float2(-reflection_dir.y,
                                           reflection_dir.x);
        float reflection_span =
            (4.0
             + 12.0 * glass_thickness
             + 0.28 * blur_points
             + 3.5 * glass_caustic_spread)
            * content_scale;
        float2 reflection_base_uv =
            refraction_uv * (0.38 + 0.42 * edge_lens);
        float2 forward_uv = clamp(
            in.screen_uv
                + reflection_base_uv
                - reflection_dir * texel * reflection_span,
            float2(0.0),
            float2(1.0));
        float2 left_uv = clamp(
            in.screen_uv
                + reflection_base_uv * 0.62
                + reflection_tangent * texel * reflection_span * 0.72,
            float2(0.0),
            float2(1.0));
        float2 right_uv = clamp(
            in.screen_uv
                + reflection_base_uv * 0.62
                - reflection_tangent * texel * reflection_span * 0.72,
            float2(0.0),
            float2(1.0));
        float3 reflected_rgb =
            backdrop.sample(samp, forward_uv).rgb * 0.50
            + backdrop.sample(samp, left_uv).rgb * 0.25
            + backdrop.sample(samp, right_uv).rgb * 0.25;
        float reflected_luma =
            dot(reflected_rgb, float3(0.2126, 0.7152, 0.0722));
        float surface_luma =
            dot(backdrop_rgb, float3(0.2126, 0.7152, 0.0722));
        float reflected_light = smoothstep(
            surface_luma - 0.12,
            surface_luma + 0.30,
            reflected_luma);
        float dark_reflection = smoothstep(
            0.08,
            0.36,
            surface_luma - reflected_luma);
        float reflection_surface =
            1.0 - smoothstep(0.22, 1.20, normalized_len);
        float reflection_sweep = smoothstep(
            -0.52,
            0.98,
            dot(normalized_local, -reflection_dir));
        float reflection_rim =
            edge_lens
            * (0.42
               + 0.58
                   * (1.0 - smoothstep(
                       0.0,
                       max(edge_width * 1.45, 0.5),
                       signed_edge_distance)));
        float reflection_gate = clamp(
            0.36 * reflection_surface * reflection_sweep
                + 0.64 * reflection_rim,
            0.0,
            1.0);
        float3 reflected_tint = mix(
            mix(float3(reflected_luma), reflected_rgb, 0.78),
            reflected_rgb * (float3(1.0) + 0.22 * in.tint.rgb),
            tint_chroma);
        rgb += reflected_tint
            * environment_reflection_strength
            * reflection_gate
            * (0.18 + 0.34 * reflected_light)
            * (0.72 + 0.28 * refraction_strength);
        float reflection_shadow = dark_reflection
            * environment_reflection_strength
            * reflection_gate
            * (0.030 + 0.044 * glass_shadow_gain);
        rgb *= 1.0 - clamp(reflection_shadow, 0.0, 0.07);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    float viscous_strain_strength = clamp(
        pointer_lens_strength * (0.42 * pointer_lens_raw + 0.58 * pointer_lens)
            + bridge_band
                * bridge_motion_strength
                * (0.36 + 0.64 * bridge_flow_offset_gain)
            + fusion_strength * (0.20 + 0.28 * group_blend_strength)
            + materialize_wave_strength * 0.26
            + control_morph_depth * 0.34,
        0.0,
        1.0);
    if (viscous_strain_strength > 0.0001) {
        float2 strain_raw_dir =
            pointer_dir
                * pointer_lens
                * pointer_lens_strength
                * 2.20
            + bridge_dir
                * bridge_band
                * bridge_motion_strength
                * (0.74 + 0.26 * union_execution)
            + refraction_dir
                * (fusion_strength * 0.52
                   + materialize_wave_strength * 0.46
                   + control_morph_depth * 0.34)
            - dynamic_light_dir * 0.18;
        float strain_dir_len = length(strain_raw_dir);
        float2 strain_dir = strain_dir_len > 0.0001
            ? strain_raw_dir / strain_dir_len
            : -dynamic_light_dir;
        float2 strain_tangent = float2(-strain_dir.y, strain_dir.x);
        float strain_span =
            (2.0
             + 4.5 * glass_thickness
             + 2.8 * glass_caustic_spread
             + 0.12 * blur_points)
            * content_scale
            * (0.70 + 0.30 * glass_lensing_gain);
        float strain_shear = clamp(
            bridge_shear * bridge_band
                + pointer_lens_raw
                    * pointer_lens_strength
                    * dot(pointer_dir, strain_tangent),
            -1.0,
            1.0);
        float2 strain_forward_uv = clamp(
            in.screen_uv
                + refraction_uv
                + strain_dir * texel * strain_span,
            float2(0.0),
            float2(1.0));
        float2 strain_back_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.56
                - strain_dir * texel * strain_span * 0.70,
            float2(0.0),
            float2(1.0));
        float2 strain_shear_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.38
                + strain_tangent
                    * texel
                    * strain_span
                    * (0.42 + 0.36 * abs(strain_shear))
                    * (strain_shear >= 0.0 ? 1.0 : -1.0),
            float2(0.0),
            float2(1.0));
        float3 strain_forward = backdrop.sample(samp, strain_forward_uv).rgb;
        float3 strain_back = backdrop.sample(samp, strain_back_uv).rgb;
        float3 strain_side = backdrop.sample(samp, strain_shear_uv).rgb;
        float3 strained_rgb =
            strain_forward * 0.44
            + strain_back * 0.34
            + strain_side * 0.22;
        float strained_luma =
            dot(strained_rgb, float3(0.2126, 0.7152, 0.0722));
        float rgb_luma = dot(rgb, float3(0.2126, 0.7152, 0.0722));
        float strain_bright = smoothstep(
            rgb_luma - 0.08,
            rgb_luma + 0.28,
            strained_luma);
        float strain_dark = smoothstep(
            0.10,
            0.34,
            rgb_luma - strained_luma);
        float strain_core =
            1.0 - smoothstep(0.14, 1.10, normalized_len);
        float strain_rim =
            edge_lens
            * (0.40
               + 0.60
                   * (1.0 - smoothstep(
                       0.0,
                       max(edge_width * 1.55, 0.5),
                       signed_edge_distance)));
        float strain_pressure_ring =
            pointer_lens_strength > 0.0001
                ? (1.0
                   - smoothstep(
                       0.0,
                       0.34,
                       abs(pointer_distance
                               / max(pointer_lens_radius, 0.001)
                           - 0.48)))
                : 0.0;
        float strain_gate = clamp(
            strain_core * 0.36
                + strain_rim * 0.42
                + strain_pressure_ring * pointer_lens_raw * 0.22
                + bridge_band * bridge_core * 0.30,
            0.0,
            1.0);
        float3 strain_tint = mix(
            mix(float3(strained_luma), strained_rgb, 0.74),
            strained_rgb * (float3(1.0) + 0.20 * in.tint.rgb),
            tint_chroma);
        rgb = mix(
            rgb,
            mix(rgb, strain_tint, 0.34 + 0.18 * strain_bright),
            viscous_strain_strength * strain_gate * 0.34);
        rgb += strain_tint
            * viscous_strain_strength
            * strain_gate
            * strain_bright
            * (0.018 + 0.036 * glass_caustic_spread);
        rgb += float3(spectral_warmth,
                      0.16 * (spectral_warmth + spectral_coolness),
                      spectral_coolness)
            * viscous_strain_strength
            * strain_gate
            * (0.012 + 0.024 * abs(strain_shear));
        float strain_shadow = strain_dark
            * viscous_strain_strength
            * strain_gate
            * (0.020 + 0.034 * glass_shadow_gain);
        rgb *= 1.0 - clamp(strain_shadow, 0.0, 0.07);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    if (bridge_band > 0.0001) {
        float bridge_rim =
            bridge_band
            * (0.38 + 0.62 * group_blend_strength)
            * (0.52 + 0.48 * edge_lens);
        float3 bridge_tint =
            float3(1.0 + spectral_warmth * 1.20,
                   1.0 + spectral_rim_tint * 0.34,
                   1.0 + spectral_coolness * 1.20);
        bridge_tint = mix(
            bridge_tint,
            bridge_tint * (float3(1.0) + in.tint.rgb * 0.36),
            tint_chroma);
        rgb += bridge_tint
            * bridge_rim
            * bridge_highlight_gain
            * (0.34 + 0.66 * bridge_motion_strength);
        rgb += bridge_tint
            * bridge_band
            * abs(bridge_shear)
            * (0.18 + 0.42 * bridge_core)
            * bridge_highlight_gain
            * bridge_flow_offset_gain
            * (0.44 + 0.56 * union_execution);
        float bridge_sidewall =
            smoothstep(0.10, 0.82, abs(bridge_shear))
            * (0.24 + 0.76 * bridge_core);
        float bridge_side_alignment =
            clamp(dot(bridge_tangent, dynamic_light_dir), -1.0, 1.0);
        float bridge_lit_side =
            clamp(0.5 + 0.5 * bridge_shear * bridge_side_alignment,
                  0.0,
                  1.0);
        float bridge_side_depth =
            bridge_band
            * bridge_sidewall
            * bridge_flow_offset_gain
            * (0.40 + 0.60 * union_execution);
        float bridge_side_shadow = clamp(
            bridge_side_depth
                * (1.0 - bridge_lit_side)
                * (0.18 + 0.12 * glass_shadow_gain),
            0.0,
            0.10);
        rgb *= 1.0 - bridge_side_shadow;
        rgb += bridge_tint
            * bridge_side_depth
            * bridge_lit_side
            * bridge_highlight_gain
            * 0.16;
        rgb = clamp(rgb, 0.0, 1.0);
    }
    float bridge_flow_strength = clamp(
        bridge_band
            * bridge_motion_strength
            * bridge_flow_offset_gain
            * (0.42 + 0.58 * union_execution)
            * (0.74 + 0.26 * glass_lensing_gain),
        0.0,
        0.72);
    if (bridge_flow_strength > 0.0001 && bridge_dir_length > 0.0001) {
        float bridge_flow_core = 1.0 - smoothstep(
            max(bridge_width * 0.18, 0.001),
            bridge_width + 0.08,
            bridge_lateral);
        float bridge_flow_edge =
            smoothstep(
                max(bridge_width * 0.28, 0.001),
                bridge_width + 0.02,
                bridge_lateral)
            * (1.0 - smoothstep(
                bridge_width + 0.02,
                bridge_width + 0.18,
                bridge_lateral));
        float bridge_axial_falloff = 1.0 - smoothstep(
            max(bridge_length * 0.55, 0.001),
            bridge_length + 0.20,
            bridge_axial);
        float bridge_flow_alignment = smoothstep(
            -0.32,
            1.0,
            dot(bridge_dir, -dynamic_light_dir));
        float bridge_shear_gloss =
            smoothstep(0.14, 0.86, abs(bridge_shear)) * bridge_core;
        float bridge_flow_gloss = bridge_flow_strength
            * bridge_axial_falloff
            * (0.54 * bridge_flow_core + 0.46 * bridge_flow_edge)
            * (0.38 + 0.62 * bridge_flow_alignment);
        float3 bridge_flow_tint =
            float3(1.0 + 1.38 * spectral_warmth,
                   1.0 + 0.38 * spectral_rim_tint,
                   1.0 + 1.38 * spectral_coolness);
        bridge_flow_tint = mix(
            bridge_flow_tint,
            bridge_flow_tint * (float3(1.0) + 0.34 * in.tint.rgb),
            tint_chroma);
        rgb += bridge_flow_tint
            * bridge_flow_gloss
            * (0.030
               + 0.046 * glass_thickness
               + 0.060 * bridge_highlight_gain);
        rgb += bridge_flow_tint
            * bridge_flow_strength
            * bridge_axial_falloff
            * bridge_shear_gloss
            * (0.018 + 0.034 * glass_caustic_spread);
        float bridge_flow_shadow = bridge_flow_strength
            * bridge_axial_falloff
            * bridge_flow_edge
            * (1.0 - bridge_flow_alignment)
            * (0.018 + 0.046 * glass_shadow_gain);
        rgb *= 1.0 - clamp(bridge_flow_shadow, 0.0, 0.10);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    float coalescence_strength = clamp(
        bridge_flow_strength * (0.34 + 0.66 * group_blend_strength)
            + bridge_band * fusion_strength * 0.30
            + bridge_band * union_execution * 0.24,
        0.0,
        0.80);
    if (coalescence_strength > 0.0001 && bridge_dir_length > 0.0001) {
        float membrane_core = 1.0 - smoothstep(
            max(bridge_width * 0.22, 0.001),
            bridge_width + 0.08,
            bridge_lateral);
        float membrane_edge =
            smoothstep(
                max(bridge_width * 0.34, 0.001),
                bridge_width + 0.04,
                bridge_lateral)
            * (1.0 - smoothstep(
                bridge_width + 0.04,
                bridge_width + 0.20,
                bridge_lateral));
        float membrane_axial = 1.0 - smoothstep(
            max(bridge_length * 0.56, 0.001),
            bridge_length + 0.22,
            bridge_axial);
        float membrane_gate = clamp(
            membrane_axial
                * (0.52 * membrane_core + 0.48 * membrane_edge)
                * (0.72 + 0.28 * edge_lens),
            0.0,
            1.0);
        float membrane_alignment = smoothstep(
            -0.28,
            1.0,
            dot(bridge_dir, -dynamic_light_dir));
        float membrane_ripple = sin(
            bridge_axial * 18.0
                + bridge_lateral_signed * 23.0
                + group_blend_strength * 7.0
                + fusion_strength * 5.0);
        float membrane_span =
            (2.0
             + 4.8 * glass_thickness
             + 3.2 * glass_caustic_spread
             + 0.10 * blur_points)
            * content_scale
            * (0.74 + 0.26 * glass_lensing_gain);
        float2 membrane_forward_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.62
                + bridge_dir * texel * membrane_span
                + bridge_tangent
                    * texel
                    * membrane_span
                    * bridge_shear
                    * (0.22 + 0.18 * membrane_ripple),
            float2(0.0),
            float2(1.0));
        float2 membrane_back_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.38
                - bridge_dir * texel * membrane_span * 0.68
                - bridge_tangent
                    * texel
                    * membrane_span
                    * bridge_shear
                    * 0.28,
            float2(0.0),
            float2(1.0));
        float3 membrane_forward =
            backdrop.sample(samp, membrane_forward_uv).rgb;
        float3 membrane_back =
            backdrop.sample(samp, membrane_back_uv).rgb;
        float3 membrane_rgb =
            membrane_forward * 0.58 + membrane_back * 0.42;
        float membrane_luma =
            dot(membrane_rgb, float3(0.2126, 0.7152, 0.0722));
        float rgb_luma = dot(rgb, float3(0.2126, 0.7152, 0.0722));
        float membrane_bright = smoothstep(
            rgb_luma - 0.08,
            rgb_luma + 0.28,
            membrane_luma);
        float membrane_dark = smoothstep(
            0.10,
            0.34,
            rgb_luma - membrane_luma);
        float3 membrane_tint = mix(
            float3(membrane_luma),
            membrane_rgb,
            0.78 + 0.12 * glass_caustic_spread);
        membrane_tint = mix(
            membrane_tint,
            membrane_tint * (float3(1.0) + 0.24 * in.tint.rgb),
            tint_chroma);
        rgb = mix(
            rgb,
            mix(rgb, membrane_tint, 0.28 + 0.20 * membrane_bright),
            coalescence_strength * membrane_gate * 0.26);
        rgb += membrane_tint
            * coalescence_strength
            * membrane_gate
            * (0.032
               + 0.052 * membrane_alignment
               + 0.030 * bridge_highlight_gain);
        rgb += float3(spectral_warmth,
                      0.18 * (spectral_warmth + spectral_coolness),
                      spectral_coolness)
            * coalescence_strength
            * membrane_gate
            * (0.018 + 0.034 * abs(bridge_shear));
        float membrane_shadow = membrane_dark
            * coalescence_strength
            * membrane_gate
            * membrane_edge
            * (0.026 + 0.044 * glass_shadow_gain);
        rgb *= 1.0 - clamp(membrane_shadow, 0.0, 0.08);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    float morph_field_strength = clamp(
        bridge_band
            * group_blend_strength
            * (0.26 + 0.36 * bridge_core)
        + coalescence_strength * 0.34
        + fusion_strength * bridge_band * 0.22
        + surface_tension_strength * 1.65,
        0.0,
        0.42);
    if (morph_field_strength > 0.0001 && bridge_dir_length > 0.0001) {
        float morph_core =
            1.0 - smoothstep(
                max(bridge_width * 0.20, 0.001),
                bridge_width + 0.10,
                bridge_lateral);
        float morph_shoulder =
            smoothstep(
                max(bridge_width * 0.36, 0.001),
                bridge_width + 0.05,
                bridge_lateral)
            * (1.0 - smoothstep(
                bridge_width + 0.05,
                bridge_width + 0.24,
                bridge_lateral));
        float morph_axial =
            1.0 - smoothstep(
                max(bridge_length * 0.58, 0.001),
                bridge_length + 0.24,
                bridge_axial);
        float morph_gate = clamp(
            morph_axial
                * (0.46 * morph_core + 0.54 * morph_shoulder)
                * (0.56 + 0.44 * edge_lens),
            0.0,
            1.0);
        float morph_alignment = smoothstep(
            -0.38,
            1.0,
            dot(bridge_dir, -dynamic_light_dir));
        float morph_phase =
            bridge_axial * (12.0 + 6.0 * group_blend_strength)
            - abs(bridge_lateral_signed)
                * (10.0 + 5.0 * bridge_flow_offset_gain)
            + fusion_strength * 8.0
            + coalescence_strength * 5.0;
        float morph_wave = 0.5 + 0.5 * sin(morph_phase);
        float morph_pinched =
            1.0 - smoothstep(0.0, 0.32, abs(morph_wave - 0.58));
        float morph_span =
            (2.2
             + 5.6 * glass_thickness
             + 4.0 * glass_caustic_spread
             + 0.12 * blur_points)
            * content_scale
            * (0.74 + 0.26 * glass_lensing_gain);
        float morph_lateral_span =
            (1.2
             + 2.8 * glass_thickness
             + 2.2 * glass_dispersion_tangential
             + 4.0 * spectral_dispersion)
            * content_scale;
        float morph_shear_sign = bridge_shear >= 0.0 ? 1.0 : -1.0;
        float2 morph_forward_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.58
                + bridge_dir
                    * texel
                    * morph_span
                    * (0.54 + 0.30 * morph_wave),
            float2(0.0),
            float2(1.0));
        float2 morph_return_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.30
                - bridge_dir
                    * texel
                    * morph_span
                    * (0.40 + 0.28 * morph_pinched),
            float2(0.0),
            float2(1.0));
        float2 morph_upper_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.42
                + bridge_tangent
                    * texel
                    * morph_lateral_span
                    * (0.64 + 0.24 * abs(bridge_shear))
                    * morph_shear_sign,
            float2(0.0),
            float2(1.0));
        float2 morph_lower_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.42
                - bridge_tangent
                    * texel
                    * morph_lateral_span
                    * (0.52 + 0.22 * morph_pinched)
                    * morph_shear_sign,
            float2(0.0),
            float2(1.0));
        float3 morph_forward =
            backdrop.sample(samp, morph_forward_uv).rgb;
        float3 morph_return =
            backdrop.sample(samp, morph_return_uv).rgb;
        float3 morph_upper =
            backdrop.sample(samp, morph_upper_uv).rgb;
        float3 morph_lower =
            backdrop.sample(samp, morph_lower_uv).rgb;
        float3 morph_rgb =
            morph_forward * 0.34
            + morph_return * 0.24
            + morph_upper * 0.22
            + morph_lower * 0.20;
        float morph_luma =
            dot(morph_rgb, float3(0.2126, 0.7152, 0.0722));
        float morph_surface_luma =
            dot(rgb, float3(0.2126, 0.7152, 0.0722));
        float morph_bright = smoothstep(
            morph_surface_luma - 0.08,
            morph_surface_luma + 0.30,
            morph_luma);
        float morph_dark = smoothstep(
            0.10,
            0.34,
            morph_surface_luma - morph_luma);
        float morph_continuity = smoothstep(
            0.04,
            0.34,
            length(morph_forward - morph_return) * 0.42
                + length(morph_upper - morph_lower) * 0.28
                + morph_pinched * 0.16);
        float3 morph_tint = mix(
            float3(morph_luma),
            morph_rgb,
            0.70 + 0.18 * glass_caustic_spread);
        morph_tint = mix(
            morph_tint,
            morph_tint * (float3(1.0) + 0.22 * in.tint.rgb),
            tint_chroma * (0.42 + 0.26 * group_blend_strength));
        morph_tint += float3(
            spectral_warmth,
            0.14 * (spectral_warmth + spectral_coolness),
            spectral_coolness)
            * (0.16 + 0.34 * spectral_rim_tint)
            * (morph_shoulder + morph_pinched * 0.42);
        float morph_weight = morph_field_strength
            * morph_gate
            * (0.38
               + 0.22 * morph_continuity
               + 0.22 * morph_bright
               + 0.18 * morph_alignment);
        rgb = mix(
            rgb,
            mix(rgb, morph_tint, 0.22 + 0.18 * morph_continuity),
            morph_weight);
        rgb += morph_tint
            * morph_weight
            * (0.018
               + 0.034 * bridge_highlight_gain
               + 0.026 * glass_prismatic_gain);
        rgb += float3(spectral_warmth,
                      0.12 * (spectral_warmth + spectral_coolness),
                      spectral_coolness)
            * morph_weight
            * morph_pinched
            * (0.016 + 0.030 * glass_caustic_spread);
        float morph_shadow = morph_dark
            * morph_field_strength
            * morph_gate
            * (0.018 + 0.038 * glass_shadow_gain)
            * (0.62 + 0.38 * (1.0 - morph_alignment));
        rgb *= 1.0 - clamp(morph_shadow, 0.0, 0.078);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    if (prominent_intensity > 0.0001) {
        float prominent_center =
            1.0 - smoothstep(0.18, 1.12, normalized_len);
        float prominent_rim =
            edge_lens
            * (0.46
               + 0.54
                   * (1.0 - smoothstep(
                       0.0,
                       max(edge_width * 1.55, 0.5),
                       signed_edge_distance)));
        float prominent_mix =
            prominent_tint_weight
            * prominent_intensity
            * (0.38 + 0.62 * prominent_center);
        rgb = mix(rgb, mix(rgb, in.tint.rgb, prominent_tint_weight),
                  prominent_mix);
        rgb += (float3(1.0) + in.tint.rgb * 0.72)
            * prominent_rim
            * prominent_edge_lift
            * (0.58 + 0.42 * prominent_intensity);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    float spectral_edge = edge_lens * spectral_rim_tint;
    float3 spectral_rgb =
        float3(spectral_warmth,
               0.24 * (spectral_warmth + spectral_coolness),
               spectral_coolness);
    float3 spectral_edge_rgb =
        spectral_rgb + tint_chromatic_rgb * (0.10 + 0.45 * spectral_edge);
    rgb += spectral_edge_rgb
        * spectral_edge
        * (0.20 + 1.40 * caustic_weight);
    float prismatic_edge = edge_lens * glass_caustic_spread;
    if (prismatic_edge > 0.0001) {
        float prism_band = smoothstep(
            -0.58,
            0.98,
            dot(normalized_local, normalize(dispersion_tangent
                                            - dynamic_light_dir * 0.38)));
        prism_band *= 1.0 - smoothstep(0.36, 1.20, normalized_len);
        float3 prism_rgb =
            float3(1.0 + spectral_warmth * 1.70,
                   1.0 + spectral_rim_tint * 0.48,
                   1.0 + spectral_coolness * 1.70);
        prism_rgb = mix(
            prism_rgb,
            prism_rgb * (float3(1.0) + in.tint.rgb * 0.42),
            tint_chroma);
        rgb += prism_rgb
            * prismatic_edge
            * prism_band
            * (0.10 + 0.34 * (glass_prismatic_gain - 1.0));
    }
    float microfacet_strength = clamp(
        0.030 * glass_thickness
            + 0.045 * glass_caustic_spread
            + 0.035 * spectral_dispersion
            + 0.020 * prominent_intensity
            + 0.040 * viscous_strain_strength
            + 0.018 * bridge_flow_strength,
        0.0,
        0.16);
    if (microfacet_strength > 0.0001) {
        float2 sparkle_grid = floor(
            (in.screen_uv
                * float2(float(backdrop.get_width()),
                         float(backdrop.get_height())))
                / max(6.0 * content_scale, 1.0));
        float sparkle_seed = fract(
            sin(dot(sparkle_grid + floor(in.local_pos * 0.03125),
                    float2(127.1, 311.7)))
            * 43758.5453);
        float sparkle_seed_b = fract(
            sin(dot(sparkle_grid + float2(19.19, 73.31),
                    float2(269.5, 183.3)))
            * 43758.5453);
        float2 facet_raw_dir =
            float2(sparkle_seed * 2.0 - 1.0,
                   sparkle_seed_b * 2.0 - 1.0);
        float facet_dir_length = length(facet_raw_dir);
        float2 facet_dir = facet_dir_length > 0.0001
            ? facet_raw_dir / facet_dir_length
            : normalize(float2(0.71, -0.70));
        float facet_alignment = pow(
            clamp(dot(facet_dir, -dynamic_light_dir) * 0.5 + 0.5,
                  0.0,
                  1.0),
            5.0);
        float facet_surface =
            1.0 - smoothstep(0.22, 1.18, normalized_len);
        float facet_edge = 1.0 - smoothstep(
            0.0,
            max(edge_width * 1.35, 0.5),
            signed_edge_distance);
        float facet_rim =
            edge_lens * (0.42 + 0.58 * facet_edge);
        float facet_gate = clamp(
            facet_surface * 0.38
                + facet_rim * 0.52
                + pointer_lens
                    * pointer_lens_strength
                    * 0.28
                + bridge_flow_strength * 0.20,
            0.0,
            1.0);
        float rare_glint = smoothstep(0.84, 0.995, sparkle_seed);
        float facet_line_phase = fract(
            dot(normalized_local, facet_dir)
                * (4.0 + 3.0 * sparkle_seed_b)
            + sparkle_seed);
        float facet_line =
            1.0 - smoothstep(
                0.0,
                0.16,
                abs(facet_line_phase - 0.5));
        float sparkle = microfacet_strength
            * facet_gate
            * (0.35 + 0.65 * facet_alignment)
            * (0.35 * facet_line + 0.65 * rare_glint);
        float2 sparkle_uv = clamp(
            in.screen_uv
                + facet_dir
                    * texel
                    * content_scale
                    * (1.5 + 2.5 * glass_caustic_spread),
            float2(0.0),
            float2(1.0));
        float3 sparkle_rgb = backdrop.sample(samp, sparkle_uv).rgb;
        float sparkle_luma =
            dot(sparkle_rgb, float3(0.2126, 0.7152, 0.0722));
        float3 microfacet_tint = mix(
            float3(sparkle_luma),
            sparkle_rgb,
            0.58 + 0.22 * glass_caustic_spread);
        microfacet_tint = mix(
            microfacet_tint,
            microfacet_tint * (float3(1.0) + 0.20 * in.tint.rgb),
            tint_chroma);
        microfacet_tint += float3(
            spectral_warmth,
            0.12 * (spectral_warmth + spectral_coolness),
            spectral_coolness)
            * (0.18 + 0.42 * spectral_rim_tint);
        rgb += microfacet_tint
            * sparkle
            * (0.030 + 0.060 * glass_prismatic_gain);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    float diffraction_strength = clamp(
        0.036 * glass_thickness
            + 0.085 * glass_caustic_spread
            + 0.060 * spectral_dispersion
            + 0.040 * (glass_prismatic_gain - 1.0)
            + 0.025 * prominent_intensity
            + 0.20 * microfacet_strength,
        0.0,
        0.18);
    if (diffraction_strength > 0.0001 && edge_bevel_width > 0.0001) {
        float diffraction_outer = 1.0 - smoothstep(
            0.0,
            max(edge_bevel_width * 1.85, 0.5),
            signed_edge_distance);
        float diffraction_inner = 1.0 - smoothstep(
            0.0,
            max(edge_width * 0.70, 0.5),
            signed_edge_distance);
        float diffraction_band = clamp(
            diffraction_outer - diffraction_inner * 0.46,
            0.0,
            1.0)
            * (0.58 + 0.42 * edge_lens);
        float2 diffraction_raw_dir =
            dispersion_tangent
                * (0.70 + 0.30 * glass_prismatic_gain)
            - dynamic_light_dir * 0.44
            + refraction_dir * 0.18;
        float diffraction_dir_length = length(diffraction_raw_dir);
        float2 diffraction_dir = diffraction_dir_length > 0.0001
            ? diffraction_raw_dir / diffraction_dir_length
            : dispersion_tangent;
        float diffraction_sweep = smoothstep(
            -0.34,
            0.92,
            dot(normalized_local, -diffraction_dir));
        float diffraction_corner = smoothstep(0.44, 1.20, normalized_len);
        float diffraction_gate = diffraction_band
            * (0.48 + 0.52 * diffraction_sweep)
            * (0.72 + 0.28 * diffraction_corner);
        float diffraction_span =
            (1.75
             + 5.5 * glass_caustic_spread
             + 1.4 * glass_dispersion_tangential
             + 0.08 * blur_points)
            * content_scale;
        float chroma_spread =
            (1.0
             + glass_dispersion_tangential * 0.70
             + spectral_dispersion * 5.0)
            * content_scale;
        float2 diffraction_warm_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.54
                + diffraction_dir * texel * diffraction_span
                + dispersion_tangent * texel * chroma_spread,
            float2(0.0),
            float2(1.0));
        float2 diffraction_cool_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.54
                - diffraction_dir * texel * diffraction_span * 0.72
                - dispersion_tangent * texel * chroma_spread,
            float2(0.0),
            float2(1.0));
        float3 diffraction_warm =
            backdrop.sample(samp, diffraction_warm_uv).rgb;
        float3 diffraction_cool =
            backdrop.sample(samp, diffraction_cool_uv).rgb;
        float3 diffraction_rgb =
            float3(diffraction_warm.r,
                   (diffraction_warm.g + diffraction_cool.g) * 0.5,
                   diffraction_cool.b);
        float diffraction_luma =
            dot(diffraction_rgb, float3(0.2126, 0.7152, 0.0722));
        float surface_luma =
            dot(rgb, float3(0.2126, 0.7152, 0.0722));
        float diffraction_bright = smoothstep(
            surface_luma - 0.08,
            surface_luma + 0.30,
            diffraction_luma);
        float diffraction_dark = smoothstep(
            0.10,
            0.34,
            surface_luma - diffraction_luma);
        float3 diffraction_tint = mix(
            float3(diffraction_luma),
            diffraction_rgb,
            0.78 + 0.12 * glass_caustic_spread);
        diffraction_tint = mix(
            diffraction_tint,
            diffraction_tint * (float3(1.0) + 0.22 * in.tint.rgb),
            tint_chroma);
        rgb += diffraction_tint
            * diffraction_strength
            * diffraction_gate
            * (0.060 + 0.080 * edge_inner_highlight)
            * (0.62 + 0.38 * diffraction_bright);
        rgb += float3(spectral_warmth,
                      0.15 * (spectral_warmth + spectral_coolness),
                      spectral_coolness)
            * diffraction_strength
            * diffraction_gate
            * (0.030 + 0.050 * glass_prismatic_gain);
        float diffraction_absorption = diffraction_dark
            * diffraction_strength
            * diffraction_gate
            * (0.018 + 0.035 * glass_shadow_gain);
        rgb *= 1.0 - clamp(diffraction_absorption, 0.0, 0.055);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    float light_sweep = smoothstep(
        -0.72,
        0.96,
        dot(normalized_local, -dynamic_light_dir));
    light_sweep *= 1.0 - smoothstep(0.28, 1.18, normalized_len);
    rgb += (float3(1.0) + spectral_rgb * 2.0 + tint_chromatic_rgb * 0.55)
        * dynamic_light_highlight
        * light_sweep
        * (0.16 * glass_scattering_gain);
    if (prominent_intensity > 0.0001) {
        rgb += (float3(1.0) + in.tint.rgb * 0.48)
            * light_sweep
            * prominent_intensity
            * 0.035;
    }
    float pointer_reaction = clamp(
        pointer_lens_strength
            * (0.46 * pointer_lens_raw + 0.54 * pointer_lens)
            * (0.72 + 0.28 * prominent_lensing_gain)
            * (0.86 + 0.14 * glass_lensing_gain),
        0.0,
        0.36);
    if (pointer_reaction > 0.0001) {
        float pointer_radius_ratio =
            pointer_distance / max(pointer_lens_radius, 0.001);
        float pointer_pressure_ring =
            1.0 - smoothstep(
                0.0,
                0.38,
                abs(pointer_radius_ratio - 0.52));
        pointer_pressure_ring *= pointer_lens_raw;
        float pointer_light_alignment = clamp(
            dot(pointer_dir, -dynamic_light_dir) * 0.5 + 0.5,
            0.0,
            1.0);
        float pointer_edge_pickup = clamp(
            edge_lens * (0.36 + 0.64 * pointer_pressure_ring),
            0.0,
            1.0);
        float3 pointer_tint =
            float3(1.0 + 1.34 * spectral_warmth,
                   1.0 + 0.40 * spectral_rim_tint,
                   1.0 + 1.34 * spectral_coolness);
        pointer_tint = mix(
            pointer_tint,
            pointer_tint * (float3(1.0) + 0.34 * in.tint.rgb),
            tint_chroma);
        float pointer_gloss = pointer_reaction
            * (0.48 + 0.52 * pointer_light_alignment)
            * (0.050 + 0.050 * glass_thickness
               + 0.050 * edge_inner_highlight);
        rgb += pointer_tint * pointer_gloss;
        rgb += pointer_tint
            * pointer_reaction
            * pointer_pressure_ring
            * (0.020 + 0.046 * glass_caustic_spread);
        rgb += pointer_tint
            * pointer_reaction
            * pointer_edge_pickup
            * (0.018 + 0.036 * edge_inner_highlight);
        float pointer_contact_shadow = pointer_reaction
            * (1.0 - pointer_light_alignment)
            * (0.018 + 0.042 * edge_outer_shadow)
            * (1.0 + dynamic_light_shadow * 0.80)
            * glass_shadow_gain;
        rgb *= 1.0 - clamp(pointer_contact_shadow, 0.0, 0.11);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    float pressure_caustic_strength = clamp(
        pointer_lens_strength * (0.36 * pointer_lens_raw + 0.64 * pointer_lens)
            + bridge_band
                * bridge_motion_strength
                * (0.30 + 0.70 * bridge_core)
            + surface_tension_strength * 2.10
            + coalescence_strength * 0.16,
        0.0,
        0.46);
    if (pressure_caustic_strength > 0.0001) {
        float pressure_radius_ratio =
            pointer_distance / max(pointer_lens_radius, 0.001);
        float pressure_pointer_ring =
            pointer_lens_strength > 0.0001
                ? (1.0
                   - smoothstep(
                       0.0,
                       0.32,
                       abs(pressure_radius_ratio - 0.48)))
                : 0.0;
        pressure_pointer_ring *= pointer_lens_raw;
        float pressure_bridge_contact =
            bridge_band * (0.40 + 0.60 * bridge_core);
        float pressure_surface =
            1.0 - smoothstep(0.16, 1.14, normalized_len);
        float pressure_rim = edge_lens
            * (0.34
               + 0.66
                   * (1.0 - smoothstep(
                       0.0,
                       max(edge_bevel_width * 1.24, 0.5),
                       signed_edge_distance)));
        float pressure_gate = clamp(
            pressure_pointer_ring * 0.42
                + pressure_bridge_contact * 0.34
                + pressure_rim * 0.26
                + pressure_surface * 0.16,
            0.0,
            1.0);
        float2 pressure_raw_dir =
            pointer_dir
                * pointer_lens
                * pointer_lens_strength
                * 1.30
            + bridge_dir
                * pressure_bridge_contact
                * (0.72 + 0.28 * union_execution)
            + refraction_dir * (0.24 + 0.40 * edge_lens)
            - dynamic_light_dir * 0.34;
        float pressure_dir_length = length(pressure_raw_dir);
        float2 pressure_dir = pressure_dir_length > 0.0001
            ? pressure_raw_dir / pressure_dir_length
            : -dynamic_light_dir;
        float2 pressure_tangent = float2(-pressure_dir.y,
                                         pressure_dir.x);
        float pressure_span =
            (1.75
             + 4.8 * glass_thickness
             + 4.2 * glass_caustic_spread
             + 0.10 * blur_points)
            * content_scale
            * (0.76 + 0.24 * glass_lensing_gain);
        float pressure_wave = sin(
            dot(normalized_local, pressure_dir) * 10.0
                + dot(normalized_local, pressure_tangent) * 4.5
                + pointer_lens_raw * 5.0
                + bridge_band * 8.0
                + surface_tension_strength * 55.0);
        float pressure_fold =
            0.5
            + 0.5
                * sin(
                    dot(normalized_local, pressure_tangent) * 12.0
                        - normalized_len * 6.0
                        + glass_caustic_spread * 23.0);
        float pressure_shear = clamp(
            bridge_shear * pressure_bridge_contact
                + pointer_lens_raw
                    * pointer_lens_strength
                    * dot(pointer_dir, pressure_tangent),
            -1.0,
            1.0);
        float2 pressure_focus_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.52
                + pressure_dir
                    * texel
                    * pressure_span
                    * (0.56 + 0.44 * pressure_wave),
            float2(0.0),
            float2(1.0));
        float2 pressure_ring_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.34
                - pressure_dir
                    * texel
                    * pressure_span
                    * (0.42 + 0.32 * pressure_fold),
            float2(0.0),
            float2(1.0));
        float2 pressure_lateral_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.30
                + pressure_tangent
                    * texel
                    * pressure_span
                    * (0.50 + 0.34 * abs(pressure_shear))
                    * (pressure_shear >= 0.0 ? 1.0 : -1.0),
            float2(0.0),
            float2(1.0));
        float3 pressure_focus =
            backdrop.sample(samp, pressure_focus_uv).rgb;
        float3 pressure_ring =
            backdrop.sample(samp, pressure_ring_uv).rgb;
        float3 pressure_lateral =
            backdrop.sample(samp, pressure_lateral_uv).rgb;
        float3 pressure_rgb =
            pressure_focus * 0.48
            + pressure_ring * 0.30
            + pressure_lateral * 0.22;
        float pressure_luma =
            dot(pressure_rgb, float3(0.2126, 0.7152, 0.0722));
        float pressure_surface_luma =
            dot(rgb, float3(0.2126, 0.7152, 0.0722));
        float pressure_bright = smoothstep(
            pressure_surface_luma - 0.08,
            pressure_surface_luma + 0.32,
            pressure_luma);
        float pressure_dark = smoothstep(
            0.10,
            0.36,
            pressure_surface_luma - pressure_luma);
        float pressure_line = smoothstep(0.18, 0.92, pressure_fold)
            * (0.72 + 0.28 * abs(pressure_wave));
        float pressure_alignment = clamp(
            dot(pressure_dir, -dynamic_light_dir) * 0.5 + 0.5,
            0.0,
            1.0);
        float pressure_focus_gate = pressure_caustic_strength
            * pressure_gate
            * (0.44 + 0.56 * pressure_line)
            * (0.50 + 0.50 * pressure_alignment);
        float3 pressure_tint = mix(
            float3(pressure_luma),
            pressure_rgb,
            0.76 + 0.14 * glass_caustic_spread);
        pressure_tint = mix(
            pressure_tint,
            pressure_tint * (float3(1.0) + 0.24 * in.tint.rgb),
            tint_chroma);
        pressure_tint += float3(
            spectral_warmth,
            0.16 * (spectral_warmth + spectral_coolness),
            spectral_coolness)
            * (0.20 + 0.48 * spectral_rim_tint);
        rgb = mix(
            rgb,
            mix(rgb, pressure_tint, 0.26 + 0.20 * pressure_bright),
            pressure_focus_gate * 0.22);
        rgb += pressure_tint
            * pressure_focus_gate
            * (0.026
               + 0.050 * glass_prismatic_gain
               + 0.038 * edge_inner_highlight);
        rgb += float3(spectral_warmth,
                      0.14 * (spectral_warmth + spectral_coolness),
                      spectral_coolness)
            * pressure_focus_gate
            * (0.018 + 0.040 * glass_caustic_spread);
        float pressure_shadow = pressure_dark
            * pressure_caustic_strength
            * pressure_gate
            * (0.018 + 0.040 * glass_shadow_gain)
            * (0.70 + 0.30 * (1.0 - pressure_alignment));
        rgb *= 1.0 - clamp(pressure_shadow, 0.0, 0.075);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    float anisotropic_highlight_strength = clamp(
        0.030 * glass_thickness
            + 0.052 * dynamic_light_highlight
            + 0.040 * bridge_flow_strength
            + 0.045 * pointer_reaction
            + 0.060 * pressure_caustic_strength
            + 0.18 * diffraction_strength
            + 0.16 * microfacet_strength
            + 0.020 * prominent_intensity,
        0.0,
        0.22);
    if (anisotropic_highlight_strength > 0.0001) {
        float2 anisotropic_raw_dir =
            bridge_dir
                * bridge_flow_strength
                * (0.90 + 0.30 * union_execution)
            + pointer_dir
                * pointer_lens
                * pointer_lens_strength
                * 0.74
            + dispersion_tangent
                * (0.22
                   + 0.48 * spectral_dispersion
                   + 0.30 * glass_caustic_spread)
            + refraction_dir * edge_lens * 0.28
            - dynamic_light_dir * 0.24;
        float anisotropic_dir_length = length(anisotropic_raw_dir);
        float2 anisotropic_dir = anisotropic_dir_length > 0.0001
            ? anisotropic_raw_dir / anisotropic_dir_length
            : normalize(float2(-dynamic_light_dir.y, dynamic_light_dir.x));
        float2 anisotropic_cross = float2(-anisotropic_dir.y,
                                          anisotropic_dir.x);
        float anisotropic_surface =
            1.0 - smoothstep(0.18, 1.18, normalized_len);
        float anisotropic_rim = edge_lens
            * (0.36
               + 0.64
                   * (1.0 - smoothstep(
                       0.0,
                       max(edge_bevel_width * 1.42, 0.5),
                       signed_edge_distance)));
        float anisotropic_contact = clamp(
            pointer_lens * pointer_lens_strength
                + bridge_band * (0.44 + 0.56 * bridge_core)
                + pressure_caustic_strength * 0.80,
            0.0,
            1.0);
        float anisotropic_alignment = smoothstep(
            -0.28,
            0.98,
            dot(anisotropic_dir, -dynamic_light_dir));
        float anisotropic_gate = clamp(
            anisotropic_surface * 0.30
                + anisotropic_rim * 0.42
                + anisotropic_contact * 0.34,
            0.0,
            1.0)
            * (0.54 + 0.46 * anisotropic_alignment);
        float anisotropic_span =
            (3.0
             + 8.4 * glass_thickness
             + 5.2 * glass_caustic_spread
             + 0.14 * blur_points)
            * content_scale
            * (0.74 + 0.26 * glass_lensing_gain);
        float anisotropic_cross_span =
            (1.0
             + 1.8 * glass_dispersion_tangential
             + 5.0 * spectral_dispersion)
            * content_scale;
        float anisotropic_phase =
            dot(normalized_local, anisotropic_dir)
                * (8.0 + 5.0 * glass_caustic_spread)
            + dot(normalized_local, anisotropic_cross) * 2.6
            + bridge_flow_strength * 5.0
            + pointer_lens * pointer_lens_strength * 4.0;
        float anisotropic_wave =
            0.5 + 0.5 * sin(anisotropic_phase);
        float anisotropic_line =
            1.0 - smoothstep(
                0.0,
                0.34,
                abs(anisotropic_wave - 0.64));
        float2 anisotropic_forward_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.50
                + anisotropic_dir
                    * texel
                    * anisotropic_span
                    * (0.58 + 0.34 * anisotropic_wave),
            float2(0.0),
            float2(1.0));
        float2 anisotropic_back_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.34
                - anisotropic_dir
                    * texel
                    * anisotropic_span
                    * (0.46 + 0.22 * anisotropic_line),
            float2(0.0),
            float2(1.0));
        float2 anisotropic_cross_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.36
                + anisotropic_cross
                    * texel
                    * anisotropic_cross_span
                    * (bridge_shear >= 0.0 ? 1.0 : -1.0)
                    * (0.74 + 0.26 * abs(bridge_shear)),
            float2(0.0),
            float2(1.0));
        float3 anisotropic_forward =
            backdrop.sample(samp, anisotropic_forward_uv).rgb;
        float3 anisotropic_back =
            backdrop.sample(samp, anisotropic_back_uv).rgb;
        float3 anisotropic_cross_rgb =
            backdrop.sample(samp, anisotropic_cross_uv).rgb;
        float3 anisotropic_rgb =
            anisotropic_forward * 0.52
            + anisotropic_back * 0.28
            + anisotropic_cross_rgb * 0.20;
        float anisotropic_luma =
            dot(anisotropic_rgb, float3(0.2126, 0.7152, 0.0722));
        float anisotropic_surface_luma =
            dot(rgb, float3(0.2126, 0.7152, 0.0722));
        float anisotropic_bright = smoothstep(
            anisotropic_surface_luma - 0.08,
            anisotropic_surface_luma + 0.30,
            anisotropic_luma);
        float anisotropic_dark = smoothstep(
            0.10,
            0.36,
            anisotropic_surface_luma - anisotropic_luma);
        float anisotropic_focus = anisotropic_highlight_strength
            * anisotropic_gate
            * (0.48
               + 0.34 * anisotropic_line
               + 0.18 * anisotropic_bright);
        float3 anisotropic_tint = mix(
            float3(anisotropic_luma),
            anisotropic_rgb,
            0.78 + 0.14 * glass_caustic_spread);
        anisotropic_tint = mix(
            anisotropic_tint,
            anisotropic_tint * (float3(1.0) + 0.22 * in.tint.rgb),
            tint_chroma);
        anisotropic_tint += float3(
            spectral_warmth,
            0.14 * (spectral_warmth + spectral_coolness),
            spectral_coolness)
            * (0.20 + 0.44 * spectral_rim_tint);
        rgb = mix(
            rgb,
            mix(rgb, anisotropic_tint, 0.18 + 0.18 * anisotropic_bright),
            anisotropic_focus * 0.18);
        rgb += anisotropic_tint
            * anisotropic_focus
            * (0.026
               + 0.050 * dynamic_light_highlight
               + 0.040 * glass_prismatic_gain);
        float anisotropic_shadow = anisotropic_dark
            * anisotropic_highlight_strength
            * anisotropic_gate
            * (0.016 + 0.036 * glass_shadow_gain)
            * (0.72 + 0.28 * (1.0 - anisotropic_alignment));
        rgb *= 1.0 - clamp(anisotropic_shadow, 0.0, 0.065);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    float ambient_chroma_bleed_strength = clamp(
        0.026 * glass_thickness
            + 0.030 * glass_caustic_spread
            + 0.026 * max(saturation - 1.0, 0.0)
            + 0.035 * bridge_flow_strength
            + 0.026 * coalescence_strength
            + 0.040 * pressure_caustic_strength
            + 0.050 * anisotropic_highlight_strength
            + 0.035 * prominent_intensity,
        0.0,
        0.18);
    if (ambient_chroma_bleed_strength > 0.0001) {
        float ambient_center =
            1.0 - smoothstep(0.18, 1.18, normalized_len);
        float ambient_rim = edge_lens
            * (0.34
               + 0.66
                   * (1.0 - smoothstep(
                       0.0,
                       max(edge_bevel_width * 1.55, 0.5),
                       signed_edge_distance)));
        float ambient_contact = clamp(
            pointer_lens * pointer_lens_strength
                + bridge_band * (0.42 + 0.58 * bridge_core)
                + pressure_caustic_strength * 0.72,
            0.0,
            1.0);
        float ambient_gate = clamp(
            ambient_center * 0.30
                + ambient_rim * 0.42
                + ambient_contact * 0.32,
            0.0,
            1.0);
        float2 ambient_raw_dir =
            refraction_dir * (0.38 + 0.42 * edge_lens)
            + bridge_dir * bridge_band * 0.48
            + pointer_dir * pointer_lens * pointer_lens_strength * 0.42
            - dynamic_light_dir * 0.24;
        float ambient_dir_length = length(ambient_raw_dir);
        float2 ambient_dir = ambient_dir_length > 0.0001
            ? ambient_raw_dir / ambient_dir_length
            : -dynamic_light_dir;
        float2 ambient_tangent = float2(-ambient_dir.y,
                                        ambient_dir.x);
        float ambient_span =
            (3.0
             + 7.6 * glass_thickness
             + 3.0 * glass_caustic_spread
             + 0.12 * blur_points)
            * content_scale
            * (0.74 + 0.26 * glass_lensing_gain);
        float ambient_chroma_span =
            (0.8
             + glass_dispersion_tangential * 1.2
             + spectral_dispersion * 4.5)
            * content_scale;
        float2 ambient_forward_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.44
                + ambient_dir * texel * ambient_span,
            float2(0.0),
            float2(1.0));
        float2 ambient_back_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.30
                - ambient_dir * texel * ambient_span * 0.64,
            float2(0.0),
            float2(1.0));
        float2 ambient_left_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.34
                + ambient_tangent * texel * ambient_span * 0.58
                + dispersion_tangent * texel * ambient_chroma_span,
            float2(0.0),
            float2(1.0));
        float2 ambient_right_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.34
                - ambient_tangent * texel * ambient_span * 0.58
                - dispersion_tangent * texel * ambient_chroma_span,
            float2(0.0),
            float2(1.0));
        float3 ambient_forward =
            backdrop.sample(samp, ambient_forward_uv).rgb;
        float3 ambient_back =
            backdrop.sample(samp, ambient_back_uv).rgb;
        float3 ambient_left =
            backdrop.sample(samp, ambient_left_uv).rgb;
        float3 ambient_right =
            backdrop.sample(samp, ambient_right_uv).rgb;
        float3 ambient_rgb =
            ambient_forward * 0.34
            + ambient_back * 0.24
            + ambient_left * 0.21
            + ambient_right * 0.21;
        float ambient_luma =
            dot(ambient_rgb, float3(0.2126, 0.7152, 0.0722));
        float ambient_surface_luma =
            dot(rgb, float3(0.2126, 0.7152, 0.0722));
        float ambient_bright = smoothstep(
            ambient_surface_luma - 0.10,
            ambient_surface_luma + 0.34,
            ambient_luma);
        float ambient_dark = smoothstep(
            0.10,
            0.36,
            ambient_surface_luma - ambient_luma);
        float ambient_chroma = max(
            max(abs(ambient_rgb.r - ambient_rgb.g),
                abs(ambient_rgb.g - ambient_rgb.b)),
            abs(ambient_rgb.b - ambient_rgb.r));
        float ambient_color_pickup = smoothstep(0.04, 0.34,
                                                ambient_chroma);
        float3 ambient_neutral =
            mix(ambient_rgb,
                float3(ambient_luma),
                0.18 + 0.30 * ambient_dark);
        float3 ambient_tint = mix(
            ambient_neutral,
            ambient_rgb,
            0.48 + 0.32 * ambient_color_pickup);
        ambient_tint = mix(
            ambient_tint,
            ambient_tint * (float3(1.0) + 0.20 * in.tint.rgb),
            tint_chroma * (0.40 + 0.30 * prominent_intensity));
        ambient_tint += float3(
            spectral_warmth,
            0.12 * (spectral_warmth + spectral_coolness),
            spectral_coolness)
            * ambient_rim
            * (0.12 + 0.34 * spectral_rim_tint);
        float ambient_weight = ambient_chroma_bleed_strength
            * ambient_gate
            * (0.46
               + 0.26 * ambient_color_pickup
               + 0.18 * ambient_bright
               + 0.10 * ambient_contact);
        rgb = mix(
            rgb,
            mix(rgb, ambient_tint, 0.22 + 0.18 * ambient_color_pickup),
            ambient_weight);
        rgb += ambient_tint
            * ambient_weight
            * (0.018
               + 0.030 * glass_prismatic_gain
               + 0.024 * dynamic_light_highlight);
        float ambient_absorption = ambient_dark
            * ambient_chroma_bleed_strength
            * ambient_gate
            * (0.014 + 0.030 * glass_shadow_gain);
        rgb *= 1.0 - clamp(ambient_absorption, 0.0, 0.055);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    float depth_parallax_strength = clamp(
        0.034 * glass_thickness
            + 0.028 * (glass_lensing_gain - 1.0)
            + 0.026 * glass_caustic_spread
            + 0.036 * ambient_chroma_bleed_strength
            + 0.044 * anisotropic_highlight_strength
            + 0.032 * pressure_caustic_strength
            + 0.020 * prominent_intensity,
        0.0,
        0.17);
    if (depth_parallax_strength > 0.0001) {
        float depth_center =
            1.0 - smoothstep(0.16, 1.20, normalized_len);
        float depth_rim = edge_lens
            * (0.30
               + 0.70
                   * (1.0 - smoothstep(
                       0.0,
                       max(edge_bevel_width * 1.40, 0.5),
                       signed_edge_distance)));
        float depth_contact = clamp(
            pointer_lens * pointer_lens_strength
                + bridge_band * (0.30 + 0.70 * bridge_core)
                + pressure_caustic_strength * 0.56,
            0.0,
            1.0);
        float depth_gate = clamp(
            depth_center * 0.34
                + depth_rim * 0.44
                + depth_contact * 0.30,
            0.0,
            1.0);
        float2 depth_raw_dir =
            refraction_dir * (0.52 + 0.48 * edge_lens)
            - dynamic_light_dir * (0.20 + 0.35 * dynamic_light_highlight)
            + pointer_dir * pointer_lens * pointer_lens_strength * 0.40
            + bridge_dir * bridge_band * 0.34;
        float depth_dir_length = length(depth_raw_dir);
        float2 depth_dir = depth_dir_length > 0.0001
            ? depth_raw_dir / depth_dir_length
            : refraction_dir;
        float2 depth_tangent = float2(-depth_dir.y, depth_dir.x);
        float depth_span =
            (2.0
             + 6.8 * glass_thickness
             + 3.8 * glass_caustic_spread
             + 0.12 * blur_points)
            * content_scale
            * (0.80 + 0.20 * glass_lensing_gain);
        float depth_split =
            (0.85
             + 2.2 * glass_thickness
             + 1.6 * pressure_caustic_strength
             + 1.0 * anisotropic_highlight_strength)
            * content_scale;
        float2 depth_front_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.50
                + depth_dir * texel * depth_span,
            float2(0.0),
            float2(1.0));
        float2 depth_middle_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.36
                + depth_tangent * texel * depth_span * 0.38,
            float2(0.0),
            float2(1.0));
        float2 depth_far_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.18
                - depth_dir * texel * depth_span
                    * (0.62 + 0.26 * depth_contact),
            float2(0.0),
            float2(1.0));
        float2 depth_chroma_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.44
                - depth_tangent * texel * depth_split
                + dispersion_tangent * texel * depth_split
                    * (0.70 + spectral_dispersion * 1.60),
            float2(0.0),
            float2(1.0));
        float3 depth_front =
            backdrop.sample(samp, depth_front_uv).rgb;
        float3 depth_middle =
            backdrop.sample(samp, depth_middle_uv).rgb;
        float3 depth_far =
            backdrop.sample(samp, depth_far_uv).rgb;
        float3 depth_chroma =
            backdrop.sample(samp, depth_chroma_uv).rgb;
        float3 depth_stack =
            depth_front * 0.34
            + depth_middle * 0.26
            + depth_far * 0.30
            + depth_chroma * 0.10;
        float depth_luma =
            dot(depth_stack, float3(0.2126, 0.7152, 0.0722));
        float depth_surface_luma =
            dot(rgb, float3(0.2126, 0.7152, 0.0722));
        float depth_light = smoothstep(
            depth_surface_luma - 0.12,
            depth_surface_luma + 0.30,
            depth_luma);
        float depth_shadow = smoothstep(
            0.08,
            0.34,
            depth_surface_luma - depth_luma);
        float depth_separation = smoothstep(
            0.02,
            0.30,
            abs(depth_luma - depth_surface_luma)
                + length(depth_front - depth_far) * 0.36);
        float3 depth_tint = mix(
            depth_stack,
            float3(depth_luma),
            depth_shadow * (0.20 + 0.22 * glass_shadow_gain));
        depth_tint = mix(
            depth_tint,
            depth_tint * (float3(1.0) + 0.16 * in.tint.rgb),
            tint_chroma * (0.34 + 0.24 * prominent_intensity));
        depth_tint += float3(
            spectral_warmth,
            0.12 * (spectral_warmth + spectral_coolness),
            spectral_coolness)
            * depth_rim
            * (0.10 + 0.28 * spectral_rim_tint);
        float depth_weight = depth_parallax_strength
            * depth_gate
            * (0.42
               + 0.22 * depth_separation
               + 0.20 * depth_light
               + 0.16 * depth_contact);
        rgb = mix(
            rgb,
            mix(rgb, depth_tint, 0.18 + 0.14 * depth_separation),
            depth_weight);
        rgb += depth_tint
            * depth_weight
            * (0.010
               + 0.024 * dynamic_light_highlight
               + 0.018 * glass_prismatic_gain);
        float depth_occlusion = depth_shadow
            * depth_weight
            * (0.032 + 0.042 * glass_shadow_gain)
            * (0.60 + 0.40 * depth_rim);
        rgb *= 1.0 - clamp(depth_occlusion, 0.0, 0.075);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    float background_extension_inflate = max(in.sampling.w, 0.0);
    float background_extension_strength = clamp(
        0.024 * glass_thickness
            + 0.024 * (glass_lensing_gain - 1.0)
            + 0.020 * glass_caustic_spread
            + 0.026 * ambient_chroma_bleed_strength
            + 0.030 * depth_parallax_strength
            + 0.020 * morph_field_strength
            + 0.0016 * background_extension_inflate,
        0.0,
        0.16);
    if (background_extension_strength > 0.0001) {
        float extension_edge = edge_lens
            * (0.38
               + 0.62
                   * (1.0 - smoothstep(
                       0.0,
                       max(edge_bevel_width * 1.65, 0.5),
                       signed_edge_distance)));
        float extension_soft = soft_edge_radius > 0.0
            ? (1.0
               - smoothstep(
                   max(soft_edge_radius * 0.18, 0.001),
                   soft_edge_radius,
                   signed_edge_distance))
            : 0.0;
        float extension_bridge = bridge_band
            * (0.40 + 0.60 * bridge_core)
            * (0.50 + 0.50 * group_blend_strength);
        float extension_gate = clamp(
            extension_edge * 0.52
                + extension_soft * 0.24
                + extension_bridge * 0.28
                + pointer_lens * pointer_lens_strength * 0.18,
            0.0,
            1.0);
        float2 extension_raw_dir =
            refraction_dir * (0.66 + 0.34 * edge_lens)
            + bridge_dir * extension_bridge * 0.52
            + pointer_dir * pointer_lens * pointer_lens_strength * 0.38
            - dynamic_light_dir * 0.18;
        float extension_dir_length = length(extension_raw_dir);
        float2 extension_dir = extension_dir_length > 0.0001
            ? extension_raw_dir / extension_dir_length
            : -dynamic_light_dir;
        float2 extension_tangent = float2(-extension_dir.y,
                                          extension_dir.x);
        float extension_span =
            (2.4
             + 5.8 * glass_thickness
             + 3.8 * glass_caustic_spread
             + 0.12 * blur_points
             + 0.16 * background_extension_inflate)
            * content_scale
            * (0.78 + 0.22 * glass_lensing_gain);
        float extension_cross_span =
            (1.2
             + 2.4 * glass_thickness
             + 1.4 * glass_dispersion_tangential
             + 3.6 * spectral_dispersion)
            * content_scale;
        float2 extension_mirror_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.36
                - extension_dir
                    * texel
                    * extension_span
                    * (0.74 + 0.26 * extension_edge),
            float2(0.0),
            float2(1.0));
        float2 extension_pull_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.52
                + extension_dir
                    * texel
                    * extension_span
                    * (0.46 + 0.24 * extension_bridge),
            float2(0.0),
            float2(1.0));
        float2 extension_left_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.42
                + extension_tangent
                    * texel
                    * extension_cross_span
                    * (0.66 + 0.24 * abs(bridge_shear))
                + dispersion_tangent
                    * texel
                    * extension_cross_span
                    * 0.30,
            float2(0.0),
            float2(1.0));
        float2 extension_right_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.42
                - extension_tangent
                    * texel
                    * extension_cross_span
                    * (0.58 + 0.22 * extension_gate)
                - dispersion_tangent
                    * texel
                    * extension_cross_span
                    * 0.24,
            float2(0.0),
            float2(1.0));
        float3 extension_mirror =
            backdrop.sample(samp, extension_mirror_uv).rgb;
        float3 extension_pull =
            backdrop.sample(samp, extension_pull_uv).rgb;
        float3 extension_left =
            backdrop.sample(samp, extension_left_uv).rgb;
        float3 extension_right =
            backdrop.sample(samp, extension_right_uv).rgb;
        float3 extension_rgb =
            extension_mirror * 0.38
            + extension_pull * 0.26
            + extension_left * 0.19
            + extension_right * 0.17;
        float extension_luma =
            dot(extension_rgb, float3(0.2126, 0.7152, 0.0722));
        float extension_surface_luma =
            dot(rgb, float3(0.2126, 0.7152, 0.0722));
        float extension_bright = smoothstep(
            extension_surface_luma - 0.09,
            extension_surface_luma + 0.32,
            extension_luma);
        float extension_dark = smoothstep(
            0.10,
            0.35,
            extension_surface_luma - extension_luma);
        float extension_detail = smoothstep(
            0.03,
            0.32,
            length(extension_mirror - extension_pull) * 0.36
                + length(extension_left - extension_right) * 0.28);
        float3 extension_neutral = mix(
            extension_rgb,
            float3(extension_luma),
            extension_dark * (0.18 + 0.22 * glass_shadow_gain));
        float3 extension_tint = mix(
            extension_neutral,
            extension_rgb,
            0.52 + 0.24 * extension_detail);
        extension_tint = mix(
            extension_tint,
            extension_tint * (float3(1.0) + 0.16 * in.tint.rgb),
            tint_chroma * (0.34 + 0.24 * prominent_intensity));
        extension_tint += float3(
            spectral_warmth,
            0.12 * (spectral_warmth + spectral_coolness),
            spectral_coolness)
            * extension_edge
            * (0.10 + 0.28 * spectral_rim_tint);
        float extension_weight = background_extension_strength
            * extension_gate
            * (0.44
               + 0.20 * extension_detail
               + 0.20 * extension_bright
               + 0.16 * extension_bridge);
        rgb = mix(
            rgb,
            mix(rgb, extension_tint, 0.20 + 0.16 * extension_detail),
            extension_weight);
        rgb += extension_tint
            * extension_weight
            * (0.010
               + 0.022 * dynamic_light_highlight
               + 0.018 * glass_prismatic_gain);
        float extension_shadow = extension_dark
            * extension_weight
            * (0.020 + 0.040 * glass_shadow_gain)
            * (0.70 + 0.30 * extension_edge);
        rgb *= 1.0 - clamp(extension_shadow, 0.0, 0.070);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    float optical_light_energy = clamp(
        dynamic_light_highlight
            + 0.34 * edge_inner_highlight
            + 0.24 * spectral_rim_tint
            + 0.18 * (glass_prismatic_gain - 1.0)
            + 0.12 * glass_caustic_spread,
        0.0,
        1.0);
    if (optical_light_energy > 0.0001) {
        float2 light_tangent = float2(-dynamic_light_dir.y,
                                      dynamic_light_dir.x);
        float directional_sheen = smoothstep(
            -0.42,
            0.92,
            dot(normalized_local, -dynamic_light_dir));
        float tangent_sheen = smoothstep(
            0.12,
            0.96,
            abs(dot(normalized_local, light_tangent)));
        float surface_gate =
            1.0 - smoothstep(0.18, 1.24, normalized_len);
        float rim_gate = edge_lens
            * (0.42
               + 0.58
                   * (1.0 - smoothstep(
                       0.0,
                       max(edge_bevel_width * 1.12, 0.5),
                       signed_edge_distance)));
        float sheen = clamp(
            surface_gate * directional_sheen * (0.55 + 0.45 * tangent_sheen)
                + 0.62 * rim_gate,
            0.0,
            1.0);
        float3 environment_tint =
            float3(1.0 + 1.45 * spectral_warmth,
                   1.0 + 0.42 * spectral_rim_tint,
                   1.0 + 1.45 * spectral_coolness);
        environment_tint = mix(
            environment_tint,
            environment_tint * (float3(1.0) + 0.36 * in.tint.rgb),
            tint_chroma);
        rgb += environment_tint
            * optical_light_energy
            * sheen
            * (0.020 + 0.045 * glass_thickness);
        float shadow_sheen = clamp(
            (1.0 - directional_sheen)
                * surface_gate
                * dynamic_light_shadow
                * (0.26 + 0.24 * glass_thickness),
            0.0,
            0.12);
        rgb *= 1.0 - shadow_sheen;
        rgb = clamp(rgb, 0.0, 1.0);
    }
    float glass_sidewall_strength = clamp(
        glass_thickness * (0.42 + 0.58 * edge_inner_highlight)
            + 0.18 * (glass_prismatic_gain - 1.0)
            + 0.12 * glass_caustic_spread,
        0.0,
        1.0);
    if (glass_sidewall_strength > 0.0001 && edge_bevel_width > 0.0001) {
        float sidewall_outer = 1.0 - smoothstep(
            0.0,
            max(edge_bevel_width * 1.35, 0.5),
            signed_edge_distance);
        float sidewall_inner = 1.0 - smoothstep(
            0.0,
            max(edge_width * 0.85, 0.5),
            signed_edge_distance);
        float sidewall_band = clamp(
            sidewall_outer - sidewall_inner * 0.54,
            0.0,
            1.0);
        float side_alignment = clamp(
            dot(refraction_dir, -dynamic_light_dir) * 0.5 + 0.5,
            0.0,
            1.0);
        float side_corner_focus = clamp(
            0.42 + 0.58 * smoothstep(0.32, 1.12, normalized_len),
            0.0,
            1.0);
        float3 side_tint =
            float3(1.0 + 1.10 * spectral_warmth,
                   1.0 + 0.28 * spectral_rim_tint,
                   1.0 + 1.10 * spectral_coolness);
        side_tint = mix(
            side_tint,
            side_tint * (float3(1.0) + 0.32 * in.tint.rgb),
            tint_chroma);
        float side_light = sidewall_band
            * glass_sidewall_strength
            * side_alignment
            * side_corner_focus
            * (0.050 + 0.070 * edge_inner_highlight);
        float side_shadow = sidewall_band
            * glass_sidewall_strength
            * (1.0 - side_alignment)
            * (0.040 + 0.060 * edge_outer_shadow)
            * glass_shadow_gain;
        rgb += side_tint * side_light;
        rgb *= 1.0 - clamp(side_shadow, 0.0, 0.16);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    float glass_meniscus_strength = clamp(
        glass_thickness * 0.38
            + (glass_lensing_gain - 1.0) * 0.42
            + refraction_strength * 0.08
            + glass_caustic_spread * 0.16,
        0.0,
        0.80);
    if (glass_meniscus_strength > 0.0001 && edge_bevel_width > 0.0001) {
        float meniscus_start = max(edge_width * 0.70, 0.5);
        float meniscus_peak = max(edge_width * 1.45,
                                  meniscus_start + 0.5);
        float meniscus_end = max(edge_bevel_width * 1.75,
                                 meniscus_peak + 0.75);
        float meniscus_band =
            smoothstep(meniscus_start,
                       meniscus_peak,
                       signed_edge_distance)
            * (1.0 - smoothstep(meniscus_peak,
                                meniscus_end,
                                signed_edge_distance))
            * edge_lens;
        float meniscus_alignment = clamp(
            dot(refraction_dir, dynamic_light_dir) * 0.5 + 0.5,
            0.0,
            1.0);
        float meniscus_luma = dot(rgb, float3(0.2126, 0.7152, 0.0722));
        rgb = mix(
            rgb,
            mix(rgb, float3(meniscus_luma), 0.24),
            meniscus_band * glass_meniscus_strength * 0.30);
        float3 meniscus_tint =
            float3(1.0 + 1.25 * spectral_warmth,
                   1.0 + 0.36 * spectral_rim_tint,
                   1.0 + 1.25 * spectral_coolness);
        meniscus_tint = mix(
            meniscus_tint,
            meniscus_tint * (float3(1.0) + 0.28 * in.tint.rgb),
            tint_chroma);
        float meniscus_light = meniscus_band
            * glass_meniscus_strength
            * (0.55 + 0.45 * meniscus_alignment)
            * (0.032 + 0.056 * edge_inner_highlight)
            * (1.0 + dynamic_light_highlight * 0.90);
        float meniscus_shadow = meniscus_band
            * glass_meniscus_strength
            * (1.0 - meniscus_alignment)
            * (0.026 + 0.050 * edge_outer_shadow)
            * (1.0 + dynamic_light_shadow * 0.80)
            * glass_shadow_gain;
        rgb += meniscus_tint * meniscus_light;
        rgb *= 1.0 - clamp(meniscus_shadow, 0.0, 0.12);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    float internal_transmission_strength = clamp(
        0.075 * glass_thickness
            + 0.035 * refraction_strength
            + 0.065 * glass_caustic_spread
            + 0.045 * (glass_lensing_gain - 1.0)
            + 0.020 * prominent_intensity,
        0.0,
        0.20);
    if (internal_transmission_strength > 0.0001
        && edge_bevel_width > 0.0001) {
        float transmission_start = max(edge_width * 0.65, 0.5);
        float transmission_peak = max(edge_width * 1.85,
                                      transmission_start + 0.5);
        float transmission_end = max(edge_bevel_width * 2.45,
                                     transmission_peak + 0.75);
        float internal_band =
            smoothstep(transmission_start,
                       transmission_peak,
                       signed_edge_distance)
            * (1.0 - smoothstep(transmission_peak,
                                transmission_end,
                                signed_edge_distance))
            * (0.50 + 0.50 * edge_lens);
        float internal_center =
            1.0 - smoothstep(0.20, 1.16, normalized_len);
        internal_band = clamp(
            internal_band + internal_center * 0.22 * glass_thickness,
            0.0,
            1.0);
        float2 transmission_raw_dir =
            refraction_dir * (0.66 + 0.34 * glass_lensing_gain)
            - dynamic_light_dir * 0.30;
        float transmission_dir_len = length(transmission_raw_dir);
        float2 transmission_dir = transmission_dir_len > 0.0001
            ? transmission_raw_dir / transmission_dir_len
            : -dynamic_light_dir;
        float transmission_span =
            (2.5
             + 7.0 * glass_thickness
             + 2.8 * glass_caustic_spread
             + 0.16 * blur_points)
            * content_scale;
        float chroma_span = max(
            1.0,
            glass_dispersion_tangential * 0.72
                + spectral_dispersion * 6.0)
            * content_scale;
        float2 warm_transmission_uv = clamp(
            in.screen_uv
                + refraction_uv
                + transmission_dir * texel * transmission_span
                + dispersion_tangent * texel * chroma_span,
            float2(0.0),
            float2(1.0));
        float2 cool_transmission_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.72
                - transmission_dir * texel * transmission_span * 0.74
                - dispersion_tangent * texel * chroma_span,
            float2(0.0),
            float2(1.0));
        float3 warm_transmission =
            backdrop.sample(samp, warm_transmission_uv).rgb;
        float3 cool_transmission =
            backdrop.sample(samp, cool_transmission_uv).rgb;
        float3 transmitted_rgb =
            float3(warm_transmission.r,
                   (warm_transmission.g + cool_transmission.g) * 0.5,
                   cool_transmission.b);
        float transmitted_luma =
            dot(transmitted_rgb, float3(0.2126, 0.7152, 0.0722));
        float surface_rgb_luma =
            dot(rgb, float3(0.2126, 0.7152, 0.0722));
        float bright_transmission = smoothstep(
            surface_rgb_luma - 0.10,
            surface_rgb_luma + 0.34,
            transmitted_luma);
        float dark_transmission = smoothstep(
            0.10,
            0.38,
            surface_rgb_luma - transmitted_luma);
        float transmission_alignment = clamp(
            dot(transmission_dir, -dynamic_light_dir) * 0.5 + 0.5,
            0.0,
            1.0);
        float3 internal_tint = mix(
            float3(transmitted_luma),
            transmitted_rgb,
            0.82);
        internal_tint = mix(
            internal_tint,
            internal_tint * (float3(1.0) + 0.24 * in.tint.rgb),
            tint_chroma);
        float transmission_glow = internal_transmission_strength
            * internal_band
            * (0.36 + 0.64 * bright_transmission)
            * (0.56 + 0.44 * transmission_alignment);
        rgb += internal_tint
            * transmission_glow
            * (0.12
               + 0.16 * edge_inner_highlight
               + 0.08 * glass_caustic_spread);
        rgb += float3(spectral_warmth,
                      0.18 * (spectral_warmth + spectral_coolness),
                      spectral_coolness)
            * transmission_glow
            * (0.08 + 0.12 * glass_prismatic_gain);
        float internal_absorption = dark_transmission
            * internal_transmission_strength
            * internal_band
            * (0.028 + 0.040 * glass_shadow_gain);
        rgb *= 1.0 - clamp(internal_absorption, 0.0, 0.06);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    float volumetric_absorption_strength = clamp(
        0.046 * glass_thickness
            + 0.030 * (glass_scattering_gain - 1.0)
            + 0.038 * glass_caustic_spread
            + 0.020 * prominent_intensity
            + 0.030 * coalescence_strength
            + 0.18 * surface_tension_strength,
        0.0,
        0.18);
    if (volumetric_absorption_strength > 0.0001) {
        float volume_center =
            1.0 - smoothstep(0.16, 1.16, normalized_len);
        float volume_rim = edge_lens
            * (0.38
               + 0.62
                   * (1.0 - smoothstep(
                       0.0,
                       max(edge_bevel_width * 1.45, 0.5),
                       signed_edge_distance)));
        float volume_contact = clamp(
            pointer_lens_raw * pointer_lens_strength
                + bridge_band * (0.48 + 0.52 * bridge_core)
                + coalescence_strength * 0.50,
            0.0,
            1.0);
        float volume_depth = clamp(
            volume_center * 0.30
                + volume_rim * 0.46
                + volume_contact * 0.28
                + internal_transmission_strength * 1.20,
            0.0,
            1.0);
        float2 volume_raw_dir =
            refraction_dir * (0.48 + 0.52 * edge_lens)
            - dynamic_light_dir * 0.42
            + bridge_dir * bridge_band * 0.34
            + pointer_dir * pointer_lens * pointer_lens_strength * 0.42;
        float volume_dir_len = length(volume_raw_dir);
        float2 volume_dir = volume_dir_len > 0.0001
            ? volume_raw_dir / volume_dir_len
            : -dynamic_light_dir;
        float2 volume_tangent = float2(-volume_dir.y, volume_dir.x);
        float volume_span =
            (2.0
             + 5.6 * glass_thickness
             + 3.6 * glass_caustic_spread
             + 0.12 * blur_points)
            * content_scale
            * (0.72 + 0.28 * glass_lensing_gain);
        float2 volume_probe_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.48
                + volume_dir * texel * volume_span
                + volume_tangent
                    * texel
                    * volume_span
                    * (bridge_shear * bridge_band * 0.22),
            float2(0.0),
            float2(1.0));
        float2 volume_shadow_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.34
                - volume_dir * texel * volume_span * 0.62,
            float2(0.0),
            float2(1.0));
        float3 volume_probe =
            backdrop.sample(samp, volume_probe_uv).rgb;
        float3 volume_shadow_probe =
            backdrop.sample(samp, volume_shadow_uv).rgb;
        float3 volume_rgb =
            volume_probe * 0.62 + volume_shadow_probe * 0.38;
        float volume_luma =
            dot(volume_rgb, float3(0.2126, 0.7152, 0.0722));
        float rgb_volume_luma =
            dot(rgb, float3(0.2126, 0.7152, 0.0722));
        float volume_bright = smoothstep(
            rgb_volume_luma - 0.10,
            rgb_volume_luma + 0.32,
            volume_luma);
        float volume_dark = smoothstep(
            0.08,
            0.34,
            rgb_volume_luma - volume_luma);
        float3 volume_tint = mix(
            float3(volume_luma),
            volume_rgb,
            0.66 + 0.18 * glass_caustic_spread);
        volume_tint = mix(
            volume_tint,
            volume_tint * (float3(1.0) + 0.20 * in.tint.rgb),
            tint_chroma);
        volume_tint += float3(
            spectral_warmth,
            0.14 * (spectral_warmth + spectral_coolness),
            spectral_coolness)
            * (0.18 + 0.36 * spectral_rim_tint);
        float volume_absorption = volumetric_absorption_strength
            * volume_depth
            * (0.52 + 0.48 * volume_dark)
            * (0.70 + 0.30 * glass_shadow_gain);
        rgb = mix(
            rgb,
            mix(rgb, volume_tint, 0.18 + 0.16 * volume_bright),
            volume_absorption * 0.34);
        rgb *= 1.0 - clamp(
            volume_absorption * (0.050 + 0.040 * volume_dark),
            0.0,
            0.075);
        rgb += volume_tint
            * volumetric_absorption_strength
            * volume_depth
            * volume_bright
            * (0.014 + 0.028 * glass_caustic_spread);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    float adaptive_contrast_strength = clamp(
        0.030 * glass_thickness
            + 0.060 * clear_glass_dimming
            + 0.075 * clear_glass_contrast
            + 0.020 * prominent_intensity
            + 0.024 * coalescence_strength
            + 0.16 * surface_tension_strength
            + 0.26 * volumetric_absorption_strength,
        0.0,
        0.16);
    if (adaptive_contrast_strength > 0.0001) {
        float contrast_center =
            1.0 - smoothstep(0.18, 1.12, normalized_len);
        float contrast_rim = edge_lens
            * (0.36
               + 0.64
                   * (1.0 - smoothstep(
                       0.0,
                       max(edge_bevel_width * 1.35, 0.5),
                       signed_edge_distance)));
        float contrast_contact = clamp(
            pointer_lens_raw * pointer_lens_strength
                + bridge_band * (0.44 + 0.56 * bridge_core),
            0.0,
            1.0);
        float contrast_gate = clamp(
            contrast_center * 0.36
                + contrast_rim * 0.48
                + contrast_contact * 0.22,
            0.0,
            1.0);
        float2 contrast_raw_dir =
            -dynamic_light_dir
            + refraction_dir * (0.34 + 0.44 * edge_lens)
            + bridge_dir * bridge_band * 0.34
            + pointer_dir * pointer_lens * pointer_lens_strength * 0.44;
        float contrast_dir_len = length(contrast_raw_dir);
        float2 contrast_dir = contrast_dir_len > 0.0001
            ? contrast_raw_dir / contrast_dir_len
            : -dynamic_light_dir;
        float2 contrast_tangent = float2(-contrast_dir.y,
                                         contrast_dir.x);
        float contrast_span =
            (2.5
             + 5.0 * glass_thickness
             + 2.0 * glass_caustic_spread
             + 0.10 * blur_points)
            * content_scale
            * (0.75 + 0.25 * prominent_lensing_gain);
        float2 contrast_forward_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.40
                + contrast_dir * texel * contrast_span,
            float2(0.0),
            float2(1.0));
        float2 contrast_back_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.26
                - contrast_dir * texel * contrast_span * 0.68,
            float2(0.0),
            float2(1.0));
        float2 contrast_side_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.32
                + contrast_tangent
                    * texel
                    * contrast_span
                    * (0.48 + 0.30 * abs(bridge_shear) * bridge_band),
            float2(0.0),
            float2(1.0));
        float3 contrast_forward =
            backdrop.sample(samp, contrast_forward_uv).rgb;
        float3 contrast_back =
            backdrop.sample(samp, contrast_back_uv).rgb;
        float3 contrast_side =
            backdrop.sample(samp, contrast_side_uv).rgb;
        float3 contrast_probe =
            contrast_forward * 0.42
            + contrast_back * 0.34
            + contrast_side * 0.24;
        float contrast_probe_luma =
            dot(contrast_probe, float3(0.2126, 0.7152, 0.0722));
        float contrast_forward_luma =
            dot(contrast_forward, float3(0.2126, 0.7152, 0.0722));
        float contrast_back_luma =
            dot(contrast_back, float3(0.2126, 0.7152, 0.0722));
        float contrast_side_luma =
            dot(contrast_side, float3(0.2126, 0.7152, 0.0722));
        float contrast_range = clamp(
            max(abs(contrast_forward_luma - contrast_back_luma),
                abs(contrast_probe_luma - contrast_side_luma)),
            0.0,
            1.0);
        float bright_pressure = smoothstep(0.60, 0.94,
                                           contrast_probe_luma);
        float dark_pressure =
            1.0 - smoothstep(0.08, 0.38, contrast_probe_luma);
        float3 contrast_neutral =
            mix(contrast_probe,
                float3(contrast_probe_luma),
                0.44 + 0.22 * contrast_range);
        float3 contrast_film = mix(
            rgb,
            contrast_neutral,
            0.16 + 0.18 * contrast_range);
        float3 compressed_film = clamp(
            (contrast_film - float3(0.50))
                    * (1.0 + clear_glass_contrast * 0.62)
                + float3(0.50),
            0.0,
            1.0);
        compressed_film = mix(
            compressed_film,
            compressed_film
                * (1.0 - 0.11 * bright_pressure)
                + float3(0.035 * dark_pressure),
            clamp(bright_pressure + dark_pressure, 0.0, 1.0));
        compressed_film +=
            float3(spectral_warmth,
                   0.16 * (spectral_warmth + spectral_coolness),
                   spectral_coolness)
            * contrast_rim
            * adaptive_contrast_strength
            * 0.34;
        compressed_film = mix(
            compressed_film,
            compressed_film * (float3(1.0) + 0.18 * in.tint.rgb),
            tint_chroma * (0.20 + 0.30 * prominent_intensity));
        float contrast_weight = adaptive_contrast_strength
            * contrast_gate
            * (0.56
               + 0.26 * contrast_range
               + 0.18 * max(bright_pressure, dark_pressure));
        rgb = mix(rgb, compressed_film, contrast_weight);
        rgb += float3(1.0)
            * contrast_rim
            * adaptive_contrast_strength
            * (0.010 + 0.018 * edge_inner_highlight)
            * (1.0 + dynamic_light_highlight);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    rgb += float3(edge * edge_lift);
    if (edge_bevel_width > 0.0001
        && (edge_inner_highlight > 0.0001 || edge_outer_shadow > 0.0001)) {
        float bevel_span = max(edge_bevel_width, edge_width);
        float bevel = 1.0 - smoothstep(
            0.0,
            max(bevel_span, 0.5),
            signed_edge_distance);
        float inner_bevel = 1.0 - smoothstep(
            0.0,
            max(edge_width * 1.25, 0.5),
            signed_edge_distance);
        float bevel_alignment = clamp(
            dot(refraction_dir, dynamic_light_dir) * 0.5 + 0.5,
            0.0,
            1.0);
        float corner_focus = clamp(length(normalized_local) * 0.52, 0.0, 1.0);
        float bright_band = inner_bevel
            * edge_inner_highlight
            * (0.34 + 0.66 * bevel_alignment)
            * (0.82 + 0.18 * corner_focus)
            * (1.0 + dynamic_light_highlight * 1.25);
        float shadow_band_inner = bevel
            * (1.0 - inner_bevel * 0.58)
            * edge_outer_shadow
            * (0.28 + 0.72 * (1.0 - bevel_alignment))
            * (1.0 + dynamic_light_shadow * 1.30)
            * glass_shadow_gain;
        shadow_band_inner = clamp(shadow_band_inner, 0.0, 0.72);
        float3 bevel_tint =
            float3(1.0 + spectral_warmth * 1.35,
                   1.0 + (spectral_warmth + spectral_coolness) * 0.22,
                   1.0 + spectral_coolness * 1.35);
        rgb += bevel_tint * bright_band;
        rgb *= (1.0 - shadow_band_inner);
    }
    float grazing_rim_strength = clamp(
        0.032 * glass_thickness
            + 0.070 * glass_caustic_spread
            + 0.040 * edge_inner_highlight
            + 0.026 * dynamic_light_highlight
            + 0.11 * pressure_caustic_strength
            + 0.18 * volumetric_absorption_strength
            + 0.12 * adaptive_contrast_strength,
        0.0,
        0.20);
    if (grazing_rim_strength > 0.0001 && edge_bevel_width > 0.0001) {
        float grazing_outer = 1.0 - smoothstep(
            0.0,
            max(edge_bevel_width * 1.80, 0.5),
            signed_edge_distance);
        float grazing_inner = 1.0 - smoothstep(
            0.0,
            max(edge_width * 0.86, 0.5),
            signed_edge_distance);
        float grazing_band = clamp(
            grazing_outer - grazing_inner * 0.62,
            0.0,
            1.0)
            * (0.54 + 0.46 * edge_lens);
        float2 grazing_tangent = float2(-refraction_dir.y,
                                        refraction_dir.x);
        float grazing_tangent_len = length(grazing_tangent);
        grazing_tangent = grazing_tangent_len > 0.0001
            ? grazing_tangent / grazing_tangent_len
            : float2(-dynamic_light_dir.y, dynamic_light_dir.x);
        float grazing_alignment = smoothstep(
            0.06,
            0.96,
            abs(dot(grazing_tangent, dynamic_light_dir)));
        float grazing_lit_side = clamp(
            dot(refraction_dir, -dynamic_light_dir) * 0.5 + 0.5,
            0.0,
            1.0);
        float grazing_corner = smoothstep(0.40, 1.20, normalized_len);
        float grazing_gate = grazing_band
            * (0.46 + 0.54 * grazing_alignment)
            * (0.76 + 0.24 * grazing_corner);
        float2 grazing_raw_dir =
            grazing_tangent
                * (grazing_lit_side >= 0.5 ? 1.0 : -1.0)
            - dynamic_light_dir * 0.36
            + refraction_dir * (0.26 + 0.30 * edge_lens);
        float grazing_dir_len = length(grazing_raw_dir);
        float2 grazing_dir = grazing_dir_len > 0.0001
            ? grazing_raw_dir / grazing_dir_len
            : grazing_tangent;
        float2 grazing_cross = float2(-grazing_dir.y, grazing_dir.x);
        float grazing_span =
            (1.6
             + 5.8 * glass_thickness
             + 5.2 * glass_caustic_spread
             + 1.8 * glass_dispersion_tangential
             + 0.08 * blur_points)
            * content_scale
            * (0.72 + 0.28 * glass_lensing_gain);
        float grazing_chroma_span =
            (0.8
             + 1.6 * glass_dispersion_tangential
             + 6.0 * spectral_dispersion)
            * content_scale;
        float grazing_wave =
            0.5
            + 0.5
                * sin(
                    dot(normalized_local, grazing_dir) * 13.0
                        + dot(normalized_local, grazing_cross) * 3.5
                        + glass_caustic_spread * 29.0
                        + surface_tension_strength * 48.0);
        float2 grazing_probe_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.44
                + grazing_dir
                    * texel
                    * grazing_span
                    * (0.52 + 0.42 * grazing_wave),
            float2(0.0),
            float2(1.0));
        float2 grazing_warm_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.42
                + grazing_dir * texel * grazing_span * 0.68
                + grazing_cross * texel * grazing_chroma_span,
            float2(0.0),
            float2(1.0));
        float2 grazing_cool_uv = clamp(
            in.screen_uv
                + refraction_uv * 0.34
                - grazing_dir * texel * grazing_span * 0.44
                - grazing_cross * texel * grazing_chroma_span,
            float2(0.0),
            float2(1.0));
        float3 grazing_probe =
            backdrop.sample(samp, grazing_probe_uv).rgb;
        float3 grazing_warm =
            backdrop.sample(samp, grazing_warm_uv).rgb;
        float3 grazing_cool =
            backdrop.sample(samp, grazing_cool_uv).rgb;
        float3 grazing_rgb =
            float3(grazing_warm.r,
                   (grazing_probe.g + grazing_warm.g + grazing_cool.g) / 3.0,
                   grazing_cool.b);
        grazing_rgb = mix(grazing_probe, grazing_rgb,
                          0.42 + 0.38 * spectral_dispersion);
        float grazing_luma =
            dot(grazing_rgb, float3(0.2126, 0.7152, 0.0722));
        float grazing_surface_luma =
            dot(rgb, float3(0.2126, 0.7152, 0.0722));
        float grazing_bright = smoothstep(
            grazing_surface_luma - 0.08,
            grazing_surface_luma + 0.30,
            grazing_luma);
        float grazing_dark = smoothstep(
            0.08,
            0.36,
            grazing_surface_luma - grazing_luma);
        float grazing_focus = grazing_rim_strength
            * grazing_gate
            * (0.58
               + 0.24 * grazing_bright
               + 0.18 * grazing_wave)
            * (0.74 + 0.26 * glass_prismatic_gain);
        float3 grazing_tint = mix(
            float3(grazing_luma),
            grazing_rgb,
            0.80 + 0.12 * glass_caustic_spread);
        grazing_tint = mix(
            grazing_tint,
            grazing_tint * (float3(1.0) + 0.24 * in.tint.rgb),
            tint_chroma);
        grazing_tint += float3(
            spectral_warmth,
            0.16 * (spectral_warmth + spectral_coolness),
            spectral_coolness)
            * (0.24 + 0.46 * spectral_rim_tint);
        rgb += grazing_tint
            * grazing_focus
            * (0.030
               + 0.058 * edge_inner_highlight
               + 0.040 * dynamic_light_highlight);
        rgb += float3(spectral_warmth,
                      0.14 * (spectral_warmth + spectral_coolness),
                      spectral_coolness)
            * grazing_focus
            * (0.018 + 0.045 * glass_caustic_spread);
        float grazing_shadow = grazing_dark
            * grazing_rim_strength
            * grazing_gate
            * (0.018 + 0.040 * edge_outer_shadow)
            * (0.70 + 0.30 * (1.0 - grazing_lit_side))
            * glass_shadow_gain;
        rgb *= 1.0 - clamp(grazing_shadow, 0.0, 0.070);
        rgb = clamp(rgb, 0.0, 1.0);
    }
    if (caustic_weight > 0.0001) {
        float rim_alignment = clamp(
            dot(refraction_dir, dynamic_light_dir) * 0.5 + 0.5,
            0.0,
            1.0);
        float rim_light = caustic_weight
            * (0.55 + 0.45 * rim_alignment * rim_alignment)
            * (0.60 + 0.40 * edge_lift)
            * (1.0 + dynamic_light_highlight * 1.35);
        float rim_shadow = caustic_weight
            * (1.0 - rim_alignment)
            * clamp(in.effects.y + 0.16, 0.0, 0.42)
            * (1.0 + dynamic_light_shadow * 1.20)
            * glass_shadow_gain;
        rim_shadow = clamp(rim_shadow, 0.0, 0.42);
        float3 rim_tint =
            float3(1.0 + spectral_warmth * 1.60,
                   1.0 + (spectral_warmth + spectral_coolness) * 0.24,
                   1.0 + spectral_coolness * 1.60);
        rgb += rim_tint * rim_light;
        rgb *= (1.0 - rim_shadow);
    }
    float specular_intensity = clamp(in.interaction.w, 0.0, 1.0);
    if (specular_intensity > 0.0) {
        float2 anchor = clamp(in.interaction.xy, float2(0.0), float2(1.0))
            * max(in.rect_size, float2(1.0));
        float specular_radius = clamp(in.interaction.z, 0.05, 1.0)
            * max(min(in.rect_size.x, in.rect_size.y), 1.0);
        float specular_distance = distance(in.local_pos, anchor);
        float glow = 1.0 - smoothstep(0.0, specular_radius, specular_distance);
        glow *= glow * specular_intensity;
        float edge_focus = edge * (1.0 - smoothstep(
            0.0,
            specular_radius * 0.75,
            specular_distance));
        rgb += float3(glow * (0.22 + 0.18 * edge_lift));
        rgb += float3(edge_focus * specular_intensity * 0.28);
    }
    float shadow_radius = clamp(in.effects.z, 0.0, 64.0);
    float shadow_band = max(edge_width, shadow_radius);
    float lower_depth = smoothstep(
        0.35,
        1.0,
        in.local_pos.y / max(in.rect_size.y, 1.0));
    float edge_depth = 1.0 - smoothstep(
        0.0,
        max(shadow_band, 0.5),
        signed_edge_distance);
    float shadow = lower_depth * edge_depth * clamp(in.effects.y, 0.0, 0.4);
    rgb *= (1.0 - shadow);
    float noise = fract(sin(dot(
        in.screen_uv * float2(float(backdrop.get_width()),
                              float(backdrop.get_height())),
        float2(12.9898, 78.233))) * 43758.5453);
    rgb += (noise - 0.5) * clamp(in.effects.w, 0.0, 0.05);
    rgb = clamp(rgb, 0.0, 1.0);
    float alpha = max(opacity, tint_strength) * edge_alpha;
    return float4(rgb, alpha);
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
    // (pivot_x, pivot_y, cos, sin) — rigid-body rotation around the
    // run's pivot. Identity packs as (any, any, 1, 0) and the
    // rotate-around-pivot expression collapses to the original
    // axis-aligned position, so untouched call sites still emit
    // pixel-identical output.
    float4 rot;
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
    // Apply the rigid rotation: subtract pivot, rotate by (cos, sin),
    // re-add pivot. Identity (cos=1, sin=0) leaves (px, py) intact.
    float dx = px - inst.rot.x;
    float dy = py - inst.rot.y;
    float rx = dx * inst.rot.z - dy * inst.rot.w;
    float ry = dx * inst.rot.w + dy * inst.rot.z;
    px = rx + inst.rot.x;
    py = ry + inst.rot.y;
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

// Image instances ride a smaller GPU struct (rect / uv_rect / color
// only — no rotation slot) so their stride stays at 48 bytes. The
// text path's `make_textured_vs` reads `inst.rot.*`, which would
// over-read the image buffer if shared. Keep them separate.
struct ImageInstance {
    float4 rect;
    float4 uv_rect;
    float4 color;
};

vertex TextVsOut vs_image(
    uint vi [[vertex_id]],
    uint ii [[instance_id]],
    constant Uniforms& u [[buffer(0)]],
    const device ImageInstance* instances [[buffer(1)]]
) {
    constexpr float2 corners[] = {
        float2(0,0), float2(1,0), float2(0,1),
        float2(1,0), float2(1,1), float2(0,1),
    };
    float2 c = corners[vi];
    ImageInstance inst = instances[ii];
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

fragment float4 fs_image(
    TextVsOut in [[stage_in]],
    texture2d<float> atlas [[texture(0)]],
    sampler samp [[sampler(0)]]
) {
    float4 sample = atlas.sample(samp, in.uv);
    if (sample.a < 0.01) discard_fragment();
    return sample * in.color;
}

// ---- SDF arc pipeline -----------------------------------------------
// Stroked arc primitive — `Cmd::DrawArc`. Vertex stage emits a six-vertex
// quad sized to the arc's stroke bbox (radius + half thickness + 1.5 px
// AA margin). Fragment stage runs an SDF: discard everything outside
// `|dist - radius| <= thickness/2` (with a smoothstep AA band) and
// outside the [start_angle, end_angle] sweep.
struct ArcInstance {
    float4 center_radius_thickness;  // cx, cy, radius, thickness
    float4 angles;                    // start, end, _, _
    float4 color;                     // linear RGBA, premultiply none
};

struct ArcVsOut {
    float4 pos [[position]];
    float2 canvas_pos;                // logical-pixel canvas coord
    float2 center;
    float2 radius_thickness;
    float2 angles;
    float4 color;
};

vertex ArcVsOut vs_arc(
    uint vi [[vertex_id]],
    uint ii [[instance_id]],
    constant Uniforms& u [[buffer(0)]],
    const device ArcInstance* instances [[buffer(1)]]
) {
    constexpr float2 corners[] = {
        float2(0,0), float2(1,0), float2(0,1),
        float2(1,0), float2(1,1), float2(0,1),
    };
    float2 c = corners[vi];
    ArcInstance inst = instances[ii];
    float2 cen = inst.center_radius_thickness.xy;
    float  r   = inst.center_radius_thickness.z;
    float  th  = inst.center_radius_thickness.w;
    float  pad = r + th * 0.5 + 1.5;

    float2 px = cen - float2(pad, pad) + c * (2.0 * pad);
    float ndcx = (px.x / u.viewport.x) * 2.0 - 1.0;
    float ndcy = 1.0 - (px.y / u.viewport.y) * 2.0;

    ArcVsOut out;
    out.pos              = float4(ndcx, ndcy, 0, 1);
    out.canvas_pos       = px;
    out.center           = cen;
    out.radius_thickness = float2(r, th);
    out.angles           = inst.angles.xy;
    out.color            = inst.color;
    return out;
}

fragment float4 fs_arc(ArcVsOut in [[stage_in]]) {
    constexpr float kTwoPi = 6.28318530717958647692;
    float2 d      = in.canvas_pos - in.center;
    float  dist   = length(d);
    float  r      = in.radius_thickness.x;
    float  th     = in.radius_thickness.y;
    float  radial = fabs(dist - r);
    float  aa     = max(fwidth(radial), 1e-3) * 0.5;
    float  alpha_r = 1.0 - smoothstep(th * 0.5 - aa, th * 0.5 + aa, radial);
    if (alpha_r <= 0.0) discard_fragment();

    float start = in.angles.x;
    float end   = in.angles.y;
    float sweep = end - start;
    if (sweep <= 0.0)  sweep += kTwoPi;
    if (sweep > kTwoPi) sweep = kTwoPi;

    // Full-circle fast path. CIRCLE entities (sweep ≈ 2π) dominate
    // cad++ DWG rendering and their angle test is tautological — skip
    // the atan / floor / branch entirely on the common path.
    if (sweep < kTwoPi - 1e-3) {
        float angle = atan2(d.y, d.x);
        float relative = angle - start;
        relative = relative - kTwoPi * floor(relative / kTwoPi);
        if (relative > sweep) discard_fragment();
    }

    return float4(in.color.rgb, in.color.a * alpha_r);
}
)";

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
