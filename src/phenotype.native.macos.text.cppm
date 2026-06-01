module;
#if !defined(__wasi__) && !defined(__ANDROID__) && defined(__APPLE__)
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <objc/runtime.h>
#endif

export module phenotype.native.macos.text;

#if !defined(__wasi__) && !defined(__ANDROID__) && defined(__APPLE__)
import cppx.unicode;
import phenotype.native.platform;
import phenotype.native.macos.objc;
import phenotype.state;
import phenotype.types;

export namespace phenotype::native::detail {

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
        key.family = ::phenotype::detail::g_app().theme.default_font_family;
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

struct MacOSTextRuntime {
    TextState state{};
};

inline MacOSTextRuntime& macos_text_runtime() {
    static MacOSTextRuntime& runtime = *new MacOSTextRuntime();
    return runtime;
}

inline TextState& text_state() {
    return macos_text_runtime().state;
}

inline bool text_runtime_initialized() noexcept {
    return text_state().initialized;
}

inline FontCacheKey font_key_from_spec(::phenotype::FontSpec const& font) {
    return FontCacheKey{
        std::string(font.family),
        font.weight,
        font.style,
        font.mono,
    };
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
    return cppx::unicode::clamp_utf8_boundary(snapshot.value, snapshot.caret_pos);
}

inline float measure_utf8_prefix(float font_size,
                                 bool mono,
                                 std::string const& text,
                                 std::size_t bytes) {
    bytes = cppx::unicode::clamp_utf8_boundary(text, bytes);
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
    auto& text = text_state();
    if (text.initialized) return;
    text.sans = create_preferred_sans_font();
    if (!text.sans)
        text.sans = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 16.0, nullptr);
    text.mono = CTFontCreateUIFontForLanguage(kCTFontUIFontUserFixedPitch, 16.0, nullptr);
    if (!text.sans) {
        std::fprintf(stderr, "[text] failed to create system font\n");
        return;
    }
    if (!text.mono)
        text.mono = static_cast<CTFontRef>(CFRetain(text.sans));
    text.initialized = true;
}

inline void text_shutdown() {
    auto& text = text_state();
    if (text.sans) { CFRelease(text.sans); text.sans = nullptr; }
    if (text.mono) { CFRelease(text.mono); text.mono = nullptr; }
    for (auto& [_, ref] : text.cache) {
        if (ref) CFRelease(ref);
    }
    text.cache.clear();
    text.missing_logged.clear();
    text.initialized = false;
}

// Resolve a base 16pt CTFontRef for `key`. Empty family resolves to
// the system sans / monospace default; named family is resolved via
// CTFontCreateWithName, then weight + italic traits are applied via
// CTFontCreateCopyWithSymbolicTraits. Failures fall back to the
// system default with a one-shot stderr log keyed on family name.
//
// The returned reference is owned by `text_state().cache`; callers must NOT
// CFRelease it. Pass to CTFontCreateCopyWithAttributes() to obtain a
// per-call sized copy (which the caller does own).
inline CTFontRef acquire_base_font(FontCacheKey const& key) {
    auto& text = text_state();
    if (!text.initialized) text_init();
    auto it = text.cache.find(key);
    if (it != text.cache.end()) return it->second;

    auto fallback = [&](char const* reason) -> CTFontRef {
        if (!key.family.empty()
            && text.missing_logged.insert(key.family).second) {
            std::fprintf(stderr,
                "[text] font '%s' (%s) not resolved (%s); using system default\n",
                key.family.c_str(),
                key.weight == ::phenotype::FontWeight::Bold ? "Bold" : "Regular",
                reason);
        }
        return key.mono ? text.mono : text.sans;
    };

    CTFontRef base = nullptr;
    if (key.family.empty()) {
        base = key.mono ? text.mono : text.sans;
        if (base) base = static_cast<CTFontRef>(CFRetain(base));
    } else {
        // Look up via process-registered alias first — a caller that
        // ran `register_font_file_macos("MyAlias", "/path/to/face.ttf")`
        // expects FontSpec{family="MyAlias"} to resolve to the
        // registered face's actual PostScript name (which is what
        // CTFontCreateWithName actually accepts after the registration
        // step).
        std::string lookup_name = key.family;
        if (auto it = text.registered_aliases.find(key.family);
            it != text.registered_aliases.end()) {
            lookup_name = it->second;
        } else if (!key.mono && key.family == "Pretendard") {
            base = text.sans
                ? static_cast<CTFontRef>(CFRetain(text.sans))
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
                if (fb) text.cache.emplace(key, static_cast<CTFontRef>(CFRetain(fb)));
                return fb;
            }
            base = CTFontCreateWithName(cf_name, 16.0, nullptr);
        }
    }
    if (!base) {
        CTFontRef fb = fallback("CTFontCreateWithName returned null");
        if (fb) text.cache.emplace(key, static_cast<CTFontRef>(CFRetain(fb)));
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
    text.cache.emplace(key, base);
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
    auto& text = text_state();
    if (!text.initialized) text_init();
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
    auto [it, inserted] = text.registered_aliases.insert_or_assign(
        alias, std::move(ps_name));
    (void)it; (void)inserted;

    // Drop any cached base-font entries that match this alias so the
    // next acquire_base_font re-resolves through the new mapping.
    for (auto cit = text.cache.begin(); cit != text.cache.end(); ) {
        if (cit->first.family == alias) {
            if (cit->second) CFRelease(cit->second);
            cit = text.cache.erase(cit);
        } else {
            ++cit;
        }
    }
    text.missing_logged.erase(alias);
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
    auto& text = text_state();
    if (!text.initialized) text_init();
    if (!text.initialized) return 0.0f;

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
    auto& text = text_state();
    if (!text.initialized) text_init();
    if (!text.initialized) return false;
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

} // namespace phenotype::native::detail
#endif
