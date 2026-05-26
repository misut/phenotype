module;
#if !defined(__wasi__) && !defined(__ANDROID__) && defined(__APPLE__)
#include <limits>
#include <objc/message.h>
#include <objc/runtime.h>
#endif

export module phenotype.native.macos.objc;

#if !defined(__wasi__) && !defined(__ANDROID__) && defined(__APPLE__)
export namespace phenotype::native::detail {

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

} // namespace phenotype::native::detail
#endif
