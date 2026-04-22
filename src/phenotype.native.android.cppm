// Android (Vulkan) platform backend for phenotype. Stage 2 cleared the
// swapchain to the active theme background; Stage 3 rendered the color
// primitives (Clear / FillRect / StrokeRect / RoundRect / DrawLine)
// via an instanced graphics pipeline. Stage 4 adds the text pipeline:
// measure glyph runs + rasterize them to an R8 atlas via JNI-backed
// android.graphics.Paint / Canvas / Bitmap.
//
// Vulkan state lives in `detail::g_renderer`; helpers in the detail
// namespace keep the module interface unit short so the Clang module
// codegen path stays simple.

module;

#if defined(__ANDROID__)
#include <cstdint>
#include <cstring>

#define VK_USE_PLATFORM_ANDROID_KHR 1
#include <vulkan/vulkan.h>
#include <android/native_window.h>
#include <android/log.h>
#include <android/bitmap.h>
#include <android/asset_manager.h>
#include <android/imagedecoder.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <fcntl.h>
#include <unistd.h>
#include <jni.h>
#endif

export module phenotype.native.android;

#if defined(__ANDROID__)
import std;
import json;
import cppx.unicode;
import phenotype;
import phenotype.diag;
import phenotype.native.platform;
import phenotype.native.shell;
import phenotype.native.stub;

namespace phenotype::native::detail::shaders {
#include "phenotype.native.android.shaders.inl"
} // namespace phenotype::native::detail::shaders

namespace phenotype::native::detail {

constexpr char const ANDROID_LOG_TAG[] = "phenotype";

// ColorInstance layout mirrored byte-for-byte with macOS
// phenotype.native.macos.cppm's ColorInstanceGPU (stride 48). The GLSL
// `struct ColorInstance` in color.vert / color.frag reads the same
// layout — keep them in sync if anything here changes.
struct ColorInstanceGPU {
    float rect[4];
    float color[4];
    float params[4];
};
static_assert(sizeof(ColorInstanceGPU) == 48,
              "ColorInstance stride must match the GLSL SSBO layout");

// TextInstance layout mirrored byte-for-byte with macOS
// phenotype.native.macos.cppm's TextInstanceGPU (stride 48). The GLSL
// `struct TextInstance` in text.vert reads the same layout.
struct TextInstanceGPU {
    float rect[4];
    float uv_rect[4];
    float color[4];
};
static_assert(sizeof(TextInstanceGPU) == 48,
              "TextInstance stride must match the GLSL SSBO layout");

// Stage 5 image instance. Layout is identical to TextInstanceGPU so
// the text pipeline's SSBO plumbing generalises — we keep the name
// distinct for readability.
using ImageInstanceGPU = TextInstanceGPU;

// Uniforms block: std140-laid-out vec2 viewport + vec2 pad (= 16B).
struct ColorUniforms {
    float viewport[2];
    float _pad[2];
};
static_assert(sizeof(ColorUniforms) == 16);

inline constexpr std::size_t INITIAL_INSTANCE_CAPACITY = 256;

// Per-frame CPU-side decode scratch. `has_clear` tracks whether the
// frame contained an explicit Clear command; when false, renderer_flush
// uses the active phenotype theme background instead.
struct FrameScratch {
    std::vector<ColorInstanceGPU> color_instances;
    std::vector<::phenotype::native::TextEntry> text_entries;
    std::vector<TextInstanceGPU> text_instances;
    std::vector<ImageInstanceGPU> image_instances;
    float clear_color[4]{0, 0, 0, 0};
    bool has_clear = false;
};

// ---- JNI text bridge --------------------------------------------------
//
// Stage 4 rasterizes glyph runs via android.graphics.Paint / Canvas /
// Bitmap. All JNI references are cached on text_init() and released on
// text_shutdown(). The GameActivity driver must call
// `phenotype_android_bind_jvm(app->activity->vm)` once before
// text_init runs — typically at the top of android_main.

struct jni_refs {
    JavaVM* vm = nullptr;

    // Classes (global refs).
    jclass paint_class         = nullptr;
    jclass typeface_class      = nullptr;
    jclass bitmap_class        = nullptr;
    jclass canvas_class        = nullptr;
    jclass bitmap_config_class = nullptr;

    // Method IDs (no cleanup needed — tied to class lifetime).
    jmethodID paint_ctor          = nullptr;
    jmethodID paint_set_textsize  = nullptr;
    jmethodID paint_set_typeface  = nullptr;
    jmethodID paint_set_color     = nullptr;
    jmethodID paint_set_antialias = nullptr;
    jmethodID paint_measure_text  = nullptr;
    jmethodID paint_ascent        = nullptr;
    jmethodID paint_descent       = nullptr;

    jmethodID bitmap_create      = nullptr;
    jmethodID bitmap_erase_color = nullptr;

    jmethodID canvas_ctor      = nullptr;
    jmethodID canvas_draw_text = nullptr;

    // Global refs to static final instances / enum values.
    jobject typeface_default   = nullptr;
    jobject typeface_monospace = nullptr;
    jobject config_argb8888    = nullptr;

    // Reusable Paint instances for the sans / mono families. Text color
    // + size get reset per call; typeface + antialias are bound once.
    jobject paint_sans = nullptr;
    jobject paint_mono = nullptr;

    bool initialised = false;
};

inline jni_refs g_jni{};

// Scoped JNIEnv helper. On a thread that's already attached (the
// GameActivity render thread is) `attached` stays false and Detach is
// a no-op; on any other thread we attach for the duration.
struct ScopedEnv {
    JNIEnv* env = nullptr;
    bool attached = false;

    explicit ScopedEnv(JavaVM* vm) {
        if (!vm) return;
        auto status = vm->GetEnv(reinterpret_cast<void**>(&env),
                                 JNI_VERSION_1_6);
        if (status == JNI_EDETACHED) {
            if (vm->AttachCurrentThread(&env, nullptr) == JNI_OK)
                attached = true;
            else
                env = nullptr;
        } else if (status != JNI_OK) {
            env = nullptr;
        }
    }

    ~ScopedEnv() {
        if (attached && g_jni.vm)
            g_jni.vm->DetachCurrentThread();
    }

    ScopedEnv(ScopedEnv const&) = delete;
    ScopedEnv& operator=(ScopedEnv const&) = delete;

    explicit operator bool() const { return env != nullptr; }
};

inline bool check_and_clear_exception(JNIEnv* env) {
    if (env && env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return true;
    }
    return false;
}

// Builds a java/lang/String from a UTF-8 byte range. Routes through
// cppx::unicode::utf8_to_utf16 so Modified-UTF-8 null-byte gotchas
// never touch JNI. On decode failure returns an empty string.
inline jstring make_jstring_utf8(JNIEnv* env, char const* text,
                                 unsigned int len) {
    if (!env) return nullptr;
    if (len == 0 || text == nullptr) return env->NewString(nullptr, 0);
    auto u16 = ::cppx::unicode::utf8_to_utf16(
        std::string_view{text, static_cast<std::size_t>(len)});
    if (!u16) return env->NewString(nullptr, 0);
    return env->NewString(
        reinterpret_cast<jchar const*>(u16->data()),
        static_cast<jsize>(u16->size()));
}

// Packs phenotype::Color (0..255 RGBA) into the 0xAARRGGBB int
// android.graphics.Color uses.
inline jint pack_android_color(float r, float g, float b, float a) {
    auto clamp = [](float v) -> unsigned {
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        return static_cast<unsigned>(v * 255.0f + 0.5f);
    };
    return static_cast<jint>(
        (clamp(a) << 24) | (clamp(r) << 16) | (clamp(g) << 8) | clamp(b));
}

inline void text_shutdown(); // forward decl

inline bool init_jni_refs(JNIEnv* env) {
    auto find_global = [&](char const* name) -> jclass {
        jclass local = env->FindClass(name);
        if (!local) {
            check_and_clear_exception(env);
            return nullptr;
        }
        auto g = reinterpret_cast<jclass>(env->NewGlobalRef(local));
        env->DeleteLocalRef(local);
        return g;
    };

    g_jni.paint_class         = find_global("android/graphics/Paint");
    g_jni.typeface_class      = find_global("android/graphics/Typeface");
    g_jni.bitmap_class        = find_global("android/graphics/Bitmap");
    g_jni.canvas_class        = find_global("android/graphics/Canvas");
    g_jni.bitmap_config_class = find_global("android/graphics/Bitmap$Config");
    if (!g_jni.paint_class || !g_jni.typeface_class || !g_jni.bitmap_class
        || !g_jni.canvas_class || !g_jni.bitmap_config_class) {
        return false;
    }

    g_jni.paint_ctor = env->GetMethodID(g_jni.paint_class, "<init>", "()V");
    g_jni.paint_set_textsize = env->GetMethodID(
        g_jni.paint_class, "setTextSize", "(F)V");
    g_jni.paint_set_typeface = env->GetMethodID(
        g_jni.paint_class, "setTypeface",
        "(Landroid/graphics/Typeface;)Landroid/graphics/Typeface;");
    g_jni.paint_set_color = env->GetMethodID(
        g_jni.paint_class, "setColor", "(I)V");
    g_jni.paint_set_antialias = env->GetMethodID(
        g_jni.paint_class, "setAntiAlias", "(Z)V");
    g_jni.paint_measure_text = env->GetMethodID(
        g_jni.paint_class, "measureText", "(Ljava/lang/String;)F");
    g_jni.paint_ascent = env->GetMethodID(
        g_jni.paint_class, "ascent", "()F");
    g_jni.paint_descent = env->GetMethodID(
        g_jni.paint_class, "descent", "()F");

    g_jni.bitmap_create = env->GetStaticMethodID(
        g_jni.bitmap_class, "createBitmap",
        "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    g_jni.bitmap_erase_color = env->GetMethodID(
        g_jni.bitmap_class, "eraseColor", "(I)V");

    g_jni.canvas_ctor = env->GetMethodID(
        g_jni.canvas_class, "<init>", "(Landroid/graphics/Bitmap;)V");
    g_jni.canvas_draw_text = env->GetMethodID(
        g_jni.canvas_class, "drawText",
        "(Ljava/lang/String;FFLandroid/graphics/Paint;)V");

    if (check_and_clear_exception(env)) return false;
    if (!g_jni.paint_ctor || !g_jni.paint_set_textsize
        || !g_jni.paint_set_typeface || !g_jni.paint_set_color
        || !g_jni.paint_set_antialias || !g_jni.paint_measure_text
        || !g_jni.paint_ascent || !g_jni.paint_descent
        || !g_jni.bitmap_create || !g_jni.bitmap_erase_color
        || !g_jni.canvas_ctor || !g_jni.canvas_draw_text)
        return false;

    // Cache Typeface.DEFAULT / Typeface.MONOSPACE as global refs.
    auto load_typeface = [&](char const* name) -> jobject {
        jfieldID id = env->GetStaticFieldID(
            g_jni.typeface_class, name, "Landroid/graphics/Typeface;");
        if (!id) { check_and_clear_exception(env); return nullptr; }
        jobject local = env->GetStaticObjectField(g_jni.typeface_class, id);
        if (!local) { check_and_clear_exception(env); return nullptr; }
        jobject g = env->NewGlobalRef(local);
        env->DeleteLocalRef(local);
        return g;
    };
    g_jni.typeface_default   = load_typeface("DEFAULT");
    g_jni.typeface_monospace = load_typeface("MONOSPACE");
    if (!g_jni.typeface_default || !g_jni.typeface_monospace) return false;

    // Bitmap.Config.ARGB_8888 — static final enum instance.
    {
        jfieldID id = env->GetStaticFieldID(
            g_jni.bitmap_config_class, "ARGB_8888",
            "Landroid/graphics/Bitmap$Config;");
        if (!id) { check_and_clear_exception(env); return false; }
        jobject local = env->GetStaticObjectField(
            g_jni.bitmap_config_class, id);
        if (!local) { check_and_clear_exception(env); return false; }
        g_jni.config_argb8888 = env->NewGlobalRef(local);
        env->DeleteLocalRef(local);
    }

    // Build the two cached Paint instances (sans / mono). Antialias
    // stays on for the lifetime; text color + size are set per call.
    auto build_paint = [&](jobject typeface) -> jobject {
        jobject local = env->NewObject(g_jni.paint_class, g_jni.paint_ctor);
        if (!local) { check_and_clear_exception(env); return nullptr; }
        env->CallVoidMethod(local, g_jni.paint_set_antialias, JNI_TRUE);
        env->CallObjectMethod(local, g_jni.paint_set_typeface, typeface);
        check_and_clear_exception(env);
        jobject g = env->NewGlobalRef(local);
        env->DeleteLocalRef(local);
        return g;
    };
    g_jni.paint_sans = build_paint(g_jni.typeface_default);
    g_jni.paint_mono = build_paint(g_jni.typeface_monospace);
    if (!g_jni.paint_sans || !g_jni.paint_mono) return false;

    return true;
}

inline void text_init() {
    if (g_jni.initialised) return;
    if (!g_jni.vm) {
        __android_log_print(
            ANDROID_LOG_WARN, ANDROID_LOG_TAG,
            "text_init skipped — phenotype_android_bind_jvm not called yet");
        return;
    }
    ScopedEnv e(g_jni.vm);
    if (!e) {
        __android_log_print(ANDROID_LOG_ERROR, ANDROID_LOG_TAG,
                            "text_init: could not attach JNIEnv");
        return;
    }
    if (init_jni_refs(e.env)) {
        g_jni.initialised = true;
    } else {
        __android_log_print(ANDROID_LOG_ERROR, ANDROID_LOG_TAG,
                            "text_init: JNI ref setup failed");
        text_shutdown();
    }
}

inline void text_shutdown() {
    if (!g_jni.vm) { g_jni.initialised = false; return; }
    ScopedEnv e(g_jni.vm);
    if (!e) { g_jni.initialised = false; return; }
    auto* env = e.env;
    auto drop = [&](jobject& ref) {
        if (ref) { env->DeleteGlobalRef(ref); ref = nullptr; }
    };
    drop(reinterpret_cast<jobject&>(g_jni.paint_class));
    drop(reinterpret_cast<jobject&>(g_jni.typeface_class));
    drop(reinterpret_cast<jobject&>(g_jni.bitmap_class));
    drop(reinterpret_cast<jobject&>(g_jni.canvas_class));
    drop(reinterpret_cast<jobject&>(g_jni.bitmap_config_class));
    drop(g_jni.typeface_default);
    drop(g_jni.typeface_monospace);
    drop(g_jni.config_argb8888);
    drop(g_jni.paint_sans);
    drop(g_jni.paint_mono);
    // Method IDs don't need cleanup — they die with the class ref.
    g_jni.paint_ctor = nullptr;
    g_jni.paint_set_textsize = nullptr;
    g_jni.paint_set_typeface = nullptr;
    g_jni.paint_set_color = nullptr;
    g_jni.paint_set_antialias = nullptr;
    g_jni.paint_measure_text = nullptr;
    g_jni.paint_ascent = nullptr;
    g_jni.paint_descent = nullptr;
    g_jni.bitmap_create = nullptr;
    g_jni.bitmap_erase_color = nullptr;
    g_jni.canvas_ctor = nullptr;
    g_jni.canvas_draw_text = nullptr;
    g_jni.initialised = false;
}

inline float text_measure(float font_size, bool mono,
                          char const* text_ptr, unsigned int len) {
    if (!g_jni.initialised || len == 0 || text_ptr == nullptr) return 0.0f;
    ScopedEnv e(g_jni.vm);
    if (!e) return 0.0f;
    auto* env = e.env;
    jobject paint = mono ? g_jni.paint_mono : g_jni.paint_sans;
    env->CallVoidMethod(paint, g_jni.paint_set_textsize,
                        static_cast<jfloat>(font_size));
    if (check_and_clear_exception(env)) return 0.0f;
    jstring s = make_jstring_utf8(env, text_ptr, len);
    if (!s) return 0.0f;
    jfloat w = env->CallFloatMethod(paint, g_jni.paint_measure_text, s);
    env->DeleteLocalRef(s);
    if (check_and_clear_exception(env)) return 0.0f;
    return static_cast<float>(w);
}

// ---- Text atlas builder ----------------------------------------------
//
// build_atlas strip-packs one slot per TextEntry into an ARGB_8888
// Bitmap, calls Canvas.drawText at the slot baseline, and reads pixels
// back via AndroidBitmap_lockPixels so we extract the alpha channel
// into the R8 atlas the Vulkan text shader expects.

inline constexpr int TEXT_ATLAS_MAX_WIDTH  = 2048;
inline constexpr int TEXT_ATLAS_MAX_HEIGHT = 4096;

// One entry's slot in the atlas prior to drawing.
struct TextSlot {
    int slot_x = 0;
    int slot_y = 0;
    int slot_w = 0;
    int slot_h = 0;
    float ascent  = 0.0f; // Paint.ascent(), negative
    float descent = 0.0f; // Paint.descent(), positive
    float pixel_width = 0.0f; // Paint.measureText() * scale, rounded up
    bool skipped = false; // true if the slot didn't fit in the atlas
};

inline int iceil(float v) { return static_cast<int>(std::ceil(v)); }

inline ::phenotype::native::TextAtlas text_build_atlas(
        std::vector<::phenotype::native::TextEntry> const& entries,
        float backing_scale) {
    using ::phenotype::native::TextAtlas;
    using ::phenotype::native::TextQuad;
    if (!g_jni.initialised || entries.empty()) return {};

    ScopedEnv e(g_jni.vm);
    if (!e) return {};
    auto* env = e.env;

    float scale = backing_scale > 0.0f ? backing_scale : 1.0f;

    // ---- pass 1: measure + layout ----
    std::vector<TextSlot> slots(entries.size());
    int cursor_x = 0;
    int cursor_y = 0;
    int row_h    = 0;
    int atlas_w  = 0;
    int atlas_h  = 0;

    for (std::size_t i = 0; i < entries.size(); ++i) {
        auto const& te = entries[i];
        TextSlot& slot = slots[i];
        jobject paint = te.mono ? g_jni.paint_mono : g_jni.paint_sans;
        env->CallVoidMethod(paint, g_jni.paint_set_textsize,
                            static_cast<jfloat>(te.font_size * scale));
        if (check_and_clear_exception(env)) { slot.skipped = true; continue; }

        slot.ascent  = env->CallFloatMethod(paint, g_jni.paint_ascent);
        slot.descent = env->CallFloatMethod(paint, g_jni.paint_descent);
        if (check_and_clear_exception(env)) { slot.skipped = true; continue; }

        jstring s = make_jstring_utf8(env, te.text.data(),
                                      static_cast<unsigned>(te.text.size()));
        jfloat measured = 0.0f;
        if (s) {
            measured = env->CallFloatMethod(
                paint, g_jni.paint_measure_text, s);
            env->DeleteLocalRef(s);
            if (check_and_clear_exception(env)) measured = 0.0f;
        }
        slot.pixel_width = measured;

        int w = iceil(measured) + 2; // 1px padding on each side
        int h = iceil(slot.descent - slot.ascent) + 2;
        if (te.line_height > 0.0f) {
            int lh = iceil(te.line_height * scale);
            if (lh > h) h = lh;
        }
        slot.slot_w = w;
        slot.slot_h = h;

        if (w > TEXT_ATLAS_MAX_WIDTH) { slot.skipped = true; continue; }
        if (cursor_x + w > TEXT_ATLAS_MAX_WIDTH) {
            cursor_x = 0;
            cursor_y += row_h;
            row_h = 0;
        }
        if (cursor_y + h > TEXT_ATLAS_MAX_HEIGHT) {
            __android_log_print(
                ANDROID_LOG_WARN, ANDROID_LOG_TAG,
                "text atlas exhausted at entry %zu (h=%d); dropping run", i, h);
            slot.skipped = true;
            continue;
        }
        slot.slot_x = cursor_x;
        slot.slot_y = cursor_y;
        cursor_x += w;
        if (h > row_h) row_h = h;
        if (cursor_x > atlas_w) atlas_w = cursor_x;
        int bottom = cursor_y + row_h;
        if (bottom > atlas_h) atlas_h = bottom;
    }

    if (atlas_w == 0 || atlas_h == 0) return {};

    // Round width up to a multiple of 4 so the Vulkan transfer doesn't
    // hit alignment issues with a 1-byte-per-texel R8 image.
    atlas_w = (atlas_w + 3) & ~3;

    // ---- pass 2: rasterize ----
    jobject bitmap = env->CallStaticObjectMethod(
        g_jni.bitmap_class, g_jni.bitmap_create,
        atlas_w, atlas_h, g_jni.config_argb8888);
    if (check_and_clear_exception(env) || !bitmap) return {};
    env->CallVoidMethod(bitmap, g_jni.bitmap_erase_color, static_cast<jint>(0));

    jobject canvas = env->NewObject(g_jni.canvas_class,
                                    g_jni.canvas_ctor, bitmap);
    if (check_and_clear_exception(env) || !canvas) {
        env->DeleteLocalRef(bitmap);
        return {};
    }

    std::vector<TextQuad> quads(entries.size());
    for (std::size_t i = 0; i < entries.size(); ++i) {
        auto const& te = entries[i];
        auto& slot = slots[i];
        if (slot.skipped) continue;

        jobject paint = te.mono ? g_jni.paint_mono : g_jni.paint_sans;
        env->CallVoidMethod(paint, g_jni.paint_set_textsize,
                            static_cast<jfloat>(te.font_size * scale));
        env->CallVoidMethod(paint, g_jni.paint_set_color,
                            pack_android_color(te.r, te.g, te.b, te.a));
        if (check_and_clear_exception(env)) continue;

        jstring s = make_jstring_utf8(env, te.text.data(),
                                      static_cast<unsigned>(te.text.size()));
        if (!s) continue;

        float baseline_y = static_cast<float>(slot.slot_y)
                         + 1.0f                  // top padding
                         + (-slot.ascent);       // ascent is negative
        env->CallVoidMethod(canvas, g_jni.canvas_draw_text, s,
                            static_cast<jfloat>(slot.slot_x + 1),
                            baseline_y, paint);
        env->DeleteLocalRef(s);
        check_and_clear_exception(env);

        // Emit the quad using entry.x / entry.y as the anchor; the
        // desktop convention places (x,y) at the pixel where the atlas
        // slot's top-left should land after downscaling.
        TextQuad& q = quads[i];
        q.x = te.x;
        q.y = te.y;
        q.w = static_cast<float>(slot.slot_w) / scale;
        q.h = static_cast<float>(slot.slot_h) / scale;
        q.u  = static_cast<float>(slot.slot_x) / static_cast<float>(atlas_w);
        q.v  = static_cast<float>(slot.slot_y) / static_cast<float>(atlas_h);
        q.uw = static_cast<float>(slot.slot_w) / static_cast<float>(atlas_w);
        q.vh = static_cast<float>(slot.slot_h) / static_cast<float>(atlas_h);
    }

    // ---- pass 3: read back pixel alpha into R8 ----
    AndroidBitmapInfo info{};
    if (AndroidBitmap_getInfo(env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS
        || info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        env->DeleteLocalRef(canvas);
        env->DeleteLocalRef(bitmap);
        return {};
    }
    void* pixel_addr = nullptr;
    if (AndroidBitmap_lockPixels(env, bitmap, &pixel_addr)
            != ANDROID_BITMAP_RESULT_SUCCESS
        || !pixel_addr) {
        env->DeleteLocalRef(canvas);
        env->DeleteLocalRef(bitmap);
        return {};
    }

    TextAtlas out;
    out.width = atlas_w;
    out.height = atlas_h;
    out.pixels.resize(static_cast<std::size_t>(atlas_w)
                    * static_cast<std::size_t>(atlas_h));
    auto const* src = static_cast<std::uint8_t const*>(pixel_addr);
    std::size_t stride = info.stride; // bytes per row, ARGB_8888 = 4 * w + padding
    for (int y = 0; y < atlas_h; ++y) {
        auto const* row = src + y * stride;
        auto* dst = out.pixels.data() + static_cast<std::size_t>(y) * atlas_w;
        for (int x = 0; x < atlas_w; ++x) {
            // ANDROID_BITMAP_FORMAT_RGBA_8888 memory layout is R,G,B,A
            // regardless of host endianness (per NDK bitmap.h contract).
            dst[x] = row[x * 4 + 3];
        }
    }
    AndroidBitmap_unlockPixels(env, bitmap);

    out.quads = std::move(quads);
    env->DeleteLocalRef(canvas);
    env->DeleteLocalRef(bitmap);
    return out;
}

// ---- Stage 5 image cache ---------------------------------------------
//
// Decodes `asset://` and absolute-path PNG / JPEG / WebP / GIF / HEIF
// inputs via NDK's AImageDecoder (API 30+; phenotype min is API 33)
// straight into an RGBA8 buffer, strip-packs each image into a single
// persistent 2048² Vulkan atlas, and stores the UV rect in a URL-
// keyed cache that never evicts. Matches macOS image_pipeline's
// persistent-atlas + dirty-region model.

enum class ImageState : unsigned char { Pending, Ready, Failed };

struct ImageCacheEntry {
    ImageState state = ImageState::Pending;
    float u  = 0.0f;
    float v  = 0.0f;
    float uw = 0.0f;
    float vh = 0.0f;
    std::string failure_detail;
};

struct DecodedImage {
    std::vector<std::uint8_t> pixels; // tightly packed RGBA8 * w * h
    std::uint32_t width  = 0;
    std::uint32_t height = 0;
};

struct ImageAtlas {
    static constexpr std::uint32_t SIZE = 2048;
    static constexpr std::uint32_t BYTES_PER_PIXEL = 4;

    // Strip-pack cursors.
    std::uint32_t cursor_x   = 0;
    std::uint32_t cursor_y   = 0;
    std::uint32_t row_height = 0;

    // Dirty-rect tracking so each frame only copies what changed.
    std::uint32_t dirty_min_x = SIZE, dirty_min_y = SIZE;
    std::uint32_t dirty_max_x = 0,    dirty_max_y = 0;
    bool has_dirty = false;

    std::vector<std::uint8_t> pixels; // SIZE * SIZE * BYTES_PER_PIXEL, lazy
    std::map<std::string, ImageCacheEntry> cache;
};

inline ImageAtlas    g_images{};
inline AAssetManager* g_asset_mgr = nullptr;

inline void mark_image_atlas_dirty(std::uint32_t x, std::uint32_t y,
                                   std::uint32_t w, std::uint32_t h) {
    if (w == 0 || h == 0) return;
    if (x < g_images.dirty_min_x) g_images.dirty_min_x = x;
    if (y < g_images.dirty_min_y) g_images.dirty_min_y = y;
    if (x + w > g_images.dirty_max_x) g_images.dirty_max_x = x + w;
    if (y + h > g_images.dirty_max_y) g_images.dirty_max_y = y + h;
    g_images.has_dirty = true;
}

// URL resolver. Returns nullopt on reject (caller marks cache Failed).
struct ResolvedImage {
    enum class Kind : unsigned char { Asset, File };
    Kind kind = Kind::File;
    std::string detail; // asset path or absolute filesystem path
};

inline std::optional<ResolvedImage> resolve_android_image(
        std::string const& url, std::string& reject_reason) {
    constexpr std::string_view asset_prefix = "asset://";
    constexpr std::string_view file_prefix  = "file://";
    constexpr std::string_view http_prefix  = "http://";
    constexpr std::string_view https_prefix = "https://";

    if (url.starts_with(asset_prefix)) {
        if (!g_asset_mgr) {
            reject_reason = "no asset manager bound";
            return std::nullopt;
        }
        ResolvedImage out;
        out.kind = ResolvedImage::Kind::Asset;
        out.detail = url.substr(asset_prefix.size());
        return out;
    }
    if (url.starts_with(http_prefix) || url.starts_with(https_prefix)) {
        reject_reason = "remote images not implemented (stage 7)";
        return std::nullopt;
    }
    std::string path = url.starts_with(file_prefix)
                         ? url.substr(file_prefix.size())
                         : url;
    if (path.empty() || path.front() != '/') {
        reject_reason = "relative paths rejected on android";
        return std::nullopt;
    }
    ResolvedImage out;
    out.kind = ResolvedImage::Kind::File;
    out.detail = std::move(path);
    return out;
}

// Configures an AImageDecoder for straight-alpha RGBA8 output and
// allocates the decoded pixel buffer. Returns nullopt on any error;
// always deletes the decoder before returning.
inline std::optional<DecodedImage> decode_with_decoder(AImageDecoder* dec) {
    struct DecoderGuard {
        AImageDecoder* d;
        ~DecoderGuard() { if (d) AImageDecoder_delete(d); }
    } guard{dec};
    if (AImageDecoder_setAndroidBitmapFormat(
            dec, ANDROID_BITMAP_FORMAT_RGBA_8888)
        != ANDROID_IMAGE_DECODER_SUCCESS)
        return std::nullopt;
    if (AImageDecoder_setUnpremultipliedRequired(dec, true)
        != ANDROID_IMAGE_DECODER_SUCCESS)
        return std::nullopt;
    auto const* info = AImageDecoder_getHeaderInfo(dec);
    if (!info) return std::nullopt;
    auto width  = AImageDecoderHeaderInfo_getWidth(info);
    auto height = AImageDecoderHeaderInfo_getHeight(info);
    if (width <= 0 || height <= 0) return std::nullopt;
    if (static_cast<std::uint32_t>(width) > ImageAtlas::SIZE
        || static_cast<std::uint32_t>(height) > ImageAtlas::SIZE)
        return std::nullopt;
    auto stride = AImageDecoder_getMinimumStride(dec);
    if (stride == 0) return std::nullopt;

    DecodedImage out;
    out.width  = static_cast<std::uint32_t>(width);
    out.height = static_cast<std::uint32_t>(height);
    std::vector<std::uint8_t> scratch(stride * static_cast<std::size_t>(height));
    auto rc = AImageDecoder_decodeImage(dec, scratch.data(), stride,
                                        scratch.size());
    if (rc != ANDROID_IMAGE_DECODER_SUCCESS) return std::nullopt;

    // Pack into tight RGBA8 (strip any row padding).
    std::size_t const row_bytes = static_cast<std::size_t>(width) * 4;
    out.pixels.resize(row_bytes * static_cast<std::size_t>(height));
    for (std::int32_t y = 0; y < height; ++y) {
        std::memcpy(out.pixels.data() + y * row_bytes,
                    scratch.data() + static_cast<std::size_t>(y) * stride,
                    row_bytes);
    }
    return out;
}

inline std::optional<DecodedImage> decode_asset_image(
        std::string const& asset_path) {
    if (!g_asset_mgr) return std::nullopt;
    AAsset* asset = AAssetManager_open(g_asset_mgr, asset_path.c_str(),
                                       AASSET_MODE_BUFFER);
    if (!asset) return std::nullopt;
    struct AssetGuard {
        AAsset* a;
        ~AssetGuard() { if (a) AAsset_close(a); }
    } guard{asset};
    auto const* buf = AAsset_getBuffer(asset);
    auto len = AAsset_getLength64(asset);
    if (!buf || len <= 0) return std::nullopt;
    AImageDecoder* dec = nullptr;
    auto rc = AImageDecoder_createFromBuffer(
        buf, static_cast<std::size_t>(len), &dec);
    if (rc != ANDROID_IMAGE_DECODER_SUCCESS || !dec) return std::nullopt;
    return decode_with_decoder(dec);
}

inline std::optional<DecodedImage> decode_file_image(
        std::string const& path) {
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) return std::nullopt;
    AImageDecoder* dec = nullptr;
    auto rc = AImageDecoder_createFromFd(fd, &dec);
    if (rc != ANDROID_IMAGE_DECODER_SUCCESS || !dec) {
        ::close(fd);
        return std::nullopt;
    }
    // createFromFd takes ownership of the fd; it's closed by delete.
    return decode_with_decoder(dec);
}

// Strip-pack a fresh image into the atlas. Returns the slot rect in
// atlas pixel coordinates; returns nullopt if the atlas is full.
struct AtlasSlot { std::uint32_t x, y, w, h; };
inline std::optional<AtlasSlot> reserve_image_slot(std::uint32_t w,
                                                   std::uint32_t h) {
    if (w == 0 || h == 0) return std::nullopt;
    if (w > ImageAtlas::SIZE || h > ImageAtlas::SIZE) return std::nullopt;
    if (g_images.cursor_x + w > ImageAtlas::SIZE) {
        g_images.cursor_x = 0;
        g_images.cursor_y += g_images.row_height;
        g_images.row_height = 0;
    }
    if (g_images.cursor_y + h > ImageAtlas::SIZE) return std::nullopt;
    AtlasSlot s{g_images.cursor_x, g_images.cursor_y, w, h};
    g_images.cursor_x += w;
    if (h > g_images.row_height) g_images.row_height = h;
    return s;
}

inline void copy_into_image_atlas(DecodedImage const& img,
                                  AtlasSlot const& slot) {
    if (g_images.pixels.empty()) {
        g_images.pixels.assign(
            static_cast<std::size_t>(ImageAtlas::SIZE)
            * ImageAtlas::SIZE * ImageAtlas::BYTES_PER_PIXEL,
            0);
    }
    std::size_t const src_row = static_cast<std::size_t>(img.width) * 4;
    std::size_t const dst_row = static_cast<std::size_t>(ImageAtlas::SIZE) * 4;
    for (std::uint32_t y = 0; y < img.height; ++y) {
        auto const* src = img.pixels.data() + y * src_row;
        auto* dst = g_images.pixels.data()
                  + (static_cast<std::size_t>(slot.y + y) * dst_row)
                  + (static_cast<std::size_t>(slot.x) * 4);
        std::memcpy(dst, src, src_row);
    }
    mark_image_atlas_dirty(slot.x, slot.y, img.width, img.height);
}

inline ImageCacheEntry const* ensure_image_cache_entry(std::string const& url) {
    auto [it, inserted] = g_images.cache.try_emplace(url, ImageCacheEntry{});
    ImageCacheEntry& entry = it->second;
    if (!inserted) return &entry;

    std::string reject_reason;
    auto resolved = resolve_android_image(url, reject_reason);
    if (!resolved) {
        entry.state = ImageState::Failed;
        entry.failure_detail = std::move(reject_reason);
        __android_log_print(ANDROID_LOG_WARN, ANDROID_LOG_TAG,
                            "image '%s' rejected: %s",
                            url.c_str(), entry.failure_detail.c_str());
        return &entry;
    }

    std::optional<DecodedImage> decoded;
    switch (resolved->kind) {
    case ResolvedImage::Kind::Asset:
        decoded = decode_asset_image(resolved->detail);
        break;
    case ResolvedImage::Kind::File:
        decoded = decode_file_image(resolved->detail);
        break;
    }
    if (!decoded) {
        entry.state = ImageState::Failed;
        entry.failure_detail = "decode failed";
        __android_log_print(ANDROID_LOG_WARN, ANDROID_LOG_TAG,
                            "image '%s' decode failed", url.c_str());
        return &entry;
    }

    auto slot = reserve_image_slot(decoded->width, decoded->height);
    if (!slot) {
        entry.state = ImageState::Failed;
        entry.failure_detail = "atlas full";
        __android_log_print(ANDROID_LOG_WARN, ANDROID_LOG_TAG,
                            "image '%s' dropped: atlas full", url.c_str());
        return &entry;
    }

    copy_into_image_atlas(*decoded, *slot);
    entry.state = ImageState::Ready;
    entry.u  = static_cast<float>(slot->x) / static_cast<float>(ImageAtlas::SIZE);
    entry.v  = static_cast<float>(slot->y) / static_cast<float>(ImageAtlas::SIZE);
    entry.uw = static_cast<float>(slot->w) / static_cast<float>(ImageAtlas::SIZE);
    entry.vh = static_cast<float>(slot->h) / static_cast<float>(ImageAtlas::SIZE);
    return &entry;
}

struct android_renderer {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    std::uint32_t queue_family_index;
    VkQueue queue;
    VkSurfaceKHR surface;

    // Swapchain-scoped — rebuilt on resize / lifecycle.
    VkSwapchainKHR swapchain;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;
    std::vector<VkFramebuffer> framebuffers;
    VkCommandBuffer command_buffer;

    // Device-scoped — created once per device, destroyed on shutdown.
    VkCommandPool command_pool;
    VkSemaphore image_available;
    VkSemaphore render_finished;
    VkFence in_flight;

    // Stage 3 color pipeline. Device-scoped; reused across swapchain
    // recreates. The framebuffers above bind to render_pass.
    VkRenderPass render_pass;
    VkDescriptorSetLayout color_descriptor_set_layout;
    VkPipelineLayout color_pipeline_layout;
    VkPipeline color_pipeline;
    VkDescriptorPool color_descriptor_pool;
    VkDescriptorSet color_descriptor_set;

    // Host-visible, persistently mapped. Uniform is fixed 16 bytes;
    // the instance buffer grows by doubling on overflow.
    VkBuffer uniform_buffer;
    VkDeviceMemory uniform_memory;
    void* uniform_mapped;

    VkBuffer instance_buffer;
    VkDeviceMemory instance_memory;
    void* instance_mapped;
    std::size_t instance_capacity; // in ColorInstanceGPU count

    // Stage 4 text pipeline. Atlas image + view grow on demand;
    // everything else is allocated once per device.
    VkDescriptorSetLayout text_descriptor_set_layout;
    VkPipelineLayout text_pipeline_layout;
    VkPipeline text_pipeline;
    VkDescriptorPool text_descriptor_pool;
    VkDescriptorSet text_descriptor_set;
    VkSampler text_sampler;

    VkImage text_atlas_image;
    VkDeviceMemory text_atlas_memory;
    VkImageView text_atlas_view;
    std::uint32_t text_atlas_width;
    std::uint32_t text_atlas_height;

    VkBuffer text_instance_buffer;
    VkDeviceMemory text_instance_memory;
    void* text_instance_mapped;
    std::size_t text_instance_capacity; // in TextInstanceGPU count

    VkBuffer text_staging_buffer;
    VkDeviceMemory text_staging_memory;
    void* text_staging_mapped;
    VkDeviceSize text_staging_capacity;

    // Stage 5 image pipeline. Atlas is a fixed-size 2048² RGBA8 image;
    // staging buffer grows on demand to fit the per-frame dirty rect.
    VkDescriptorSetLayout image_descriptor_set_layout;
    VkPipelineLayout image_pipeline_layout;
    VkPipeline image_pipeline;
    VkDescriptorPool image_descriptor_pool;
    VkDescriptorSet image_descriptor_set;
    VkSampler image_sampler;

    VkImage image_atlas_image;
    VkDeviceMemory image_atlas_memory;
    VkImageView image_atlas_view;
    bool image_atlas_initialised;

    VkBuffer image_instance_buffer;
    VkDeviceMemory image_instance_memory;
    void* image_instance_mapped;
    std::size_t image_instance_capacity; // in ImageInstanceGPU count

    VkBuffer image_staging_buffer;
    VkDeviceMemory image_staging_memory;
    void* image_staging_mapped;
    VkDeviceSize image_staging_capacity;

    // Stage 6: keep a copy of the last command buffer that renderer_flush
    // submitted so hit_test can walk its HitRegionCmd list without
    // forcing a re-render on every pointer dispatch.
    std::vector<unsigned char> last_frame_buf;

    // Stage 7: persistent debug-capture image. Each renderer_flush runs
    // `vkCmdCopyImage` from the presented swapchain image into this
    // target right after the render pass ends, so capture_frame_rgba()
    // can read a fresh snapshot without re-rendering. Mirrors macOS's
    // debug_capture_texture.
    VkImage debug_capture_image;
    VkDeviceMemory debug_capture_memory;
    VkImageView debug_capture_view;
    VkBuffer debug_readback_buffer;
    VkDeviceMemory debug_readback_memory;
    void* debug_readback_mapped;
    VkDeviceSize debug_readback_capacity;
    std::uint32_t last_render_width;
    std::uint32_t last_render_height;
    bool last_frame_available;
    bool debug_capture_ready; // image has received at least one copy

    ANativeWindow* window;
    bool initialized;
};

inline android_renderer g_renderer{};

inline bool vk_ok(VkResult r, char const* label) {
    if (r == VK_SUCCESS) return true;
    __android_log_print(ANDROID_LOG_ERROR, ANDROID_LOG_TAG,
                        "%s failed (VkResult=%d)", label, static_cast<int>(r));
    return false;
}

inline std::optional<std::uint32_t> find_memory_type(
        std::uint32_t type_filter, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties props{};
    vkGetPhysicalDeviceMemoryProperties(g_renderer.physical_device, &props);
    for (std::uint32_t i = 0; i < props.memoryTypeCount; ++i) {
        if ((type_filter & (1u << i))
            && (props.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    return std::nullopt;
}

// Allocates a host-visible, host-coherent, persistently-mapped buffer
// of the requested size + usage. Returns false on failure.
inline bool create_host_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                               VkBuffer& out_buffer,
                               VkDeviceMemory& out_memory,
                               void*& out_mapped) {
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (!vk_ok(vkCreateBuffer(g_renderer.device, &bi, nullptr, &out_buffer),
              "vkCreateBuffer"))
        return false;

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(g_renderer.device, out_buffer, &req);
    auto mt = find_memory_type(req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!mt) {
        __android_log_print(ANDROID_LOG_ERROR, ANDROID_LOG_TAG,
                            "no host-visible memory type for buffer usage=%u",
                            static_cast<unsigned>(usage));
        return false;
    }

    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = *mt;
    if (!vk_ok(vkAllocateMemory(g_renderer.device, &ai, nullptr, &out_memory),
              "vkAllocateMemory"))
        return false;
    if (!vk_ok(vkBindBufferMemory(g_renderer.device, out_buffer, out_memory, 0),
              "vkBindBufferMemory"))
        return false;
    if (!vk_ok(vkMapMemory(g_renderer.device, out_memory, 0, VK_WHOLE_SIZE, 0,
                           &out_mapped),
              "vkMapMemory"))
        return false;
    return true;
}

inline void destroy_host_buffer(VkBuffer& buffer, VkDeviceMemory& memory,
                                void*& mapped) {
    if (memory != VK_NULL_HANDLE && mapped != nullptr) {
        vkUnmapMemory(g_renderer.device, memory);
        mapped = nullptr;
    }
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(g_renderer.device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_renderer.device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

// Rewrites the single descriptor set to point at the current uniform
// + instance buffers. Called on device init and whenever the instance
// buffer is reallocated after a capacity overflow.
inline void write_color_descriptor_set() {
    if (g_renderer.color_descriptor_set == VK_NULL_HANDLE) return;
    VkDescriptorBufferInfo ubo{};
    ubo.buffer = g_renderer.uniform_buffer;
    ubo.offset = 0;
    ubo.range = sizeof(ColorUniforms);

    VkDescriptorBufferInfo ssbo{};
    ssbo.buffer = g_renderer.instance_buffer;
    ssbo.offset = 0;
    ssbo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = g_renderer.color_descriptor_set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &ubo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = g_renderer.color_descriptor_set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &ssbo;

    vkUpdateDescriptorSets(g_renderer.device, 2, writes, 0, nullptr);
}

// Ensures the instance buffer can hold at least `required` instances.
// Grows by doubling. Rewrites the descriptor set when the underlying
// buffer is reallocated. Returns false on allocation failure.
inline bool ensure_instance_capacity(std::size_t required) {
    if (required <= g_renderer.instance_capacity) return true;
    std::size_t cap = g_renderer.instance_capacity == 0
                        ? INITIAL_INSTANCE_CAPACITY
                        : g_renderer.instance_capacity;
    while (cap < required) cap *= 2;

    // GPU may still be consuming the old buffer; the caller guarantees
    // the in-flight fence has been waited on before calling this.
    destroy_host_buffer(g_renderer.instance_buffer,
                        g_renderer.instance_memory,
                        g_renderer.instance_mapped);

    if (!create_host_buffer(cap * sizeof(ColorInstanceGPU),
                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                            g_renderer.instance_buffer,
                            g_renderer.instance_memory,
                            g_renderer.instance_mapped))
        return false;
    g_renderer.instance_capacity = cap;
    write_color_descriptor_set();
    return true;
}

inline VkShaderModule create_shader_module(uint32_t const* code,
                                           std::size_t size_bytes) {
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = size_bytes;
    ci.pCode = code;
    VkShaderModule mod = VK_NULL_HANDLE;
    vk_ok(vkCreateShaderModule(g_renderer.device, &ci, nullptr, &mod),
          "vkCreateShaderModule");
    return mod;
}

inline bool create_render_pass() {
    if (g_renderer.render_pass != VK_NULL_HANDLE) return true;

    VkAttachmentDescription att{};
    att.format = g_renderer.swapchain_format;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ref{};
    ref.attachment = 0;
    ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ref;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    ci.attachmentCount = 1;
    ci.pAttachments = &att;
    ci.subpassCount = 1;
    ci.pSubpasses = &sub;
    ci.dependencyCount = 1;
    ci.pDependencies = &dep;

    return vk_ok(vkCreateRenderPass(g_renderer.device, &ci, nullptr,
                                    &g_renderer.render_pass),
                "vkCreateRenderPass");
}

inline bool create_color_pipeline() {
    if (g_renderer.color_pipeline != VK_NULL_HANDLE) return true;

    // Descriptor set layout: binding 0 = UBO (vertex), binding 1 = SSBO (vertex).
    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo dslci{};
    dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = 2;
    dslci.pBindings = bindings;
    if (!vk_ok(vkCreateDescriptorSetLayout(
                  g_renderer.device, &dslci, nullptr,
                  &g_renderer.color_descriptor_set_layout),
              "vkCreateDescriptorSetLayout"))
        return false;

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &g_renderer.color_descriptor_set_layout;
    if (!vk_ok(vkCreatePipelineLayout(g_renderer.device, &plci, nullptr,
                                      &g_renderer.color_pipeline_layout),
              "vkCreatePipelineLayout"))
        return false;

    auto vs = create_shader_module(shaders::SPIRV_COLOR_VS,
                                   sizeof(shaders::SPIRV_COLOR_VS));
    auto fs = create_shader_module(shaders::SPIRV_COLOR_FS,
                                   sizeof(shaders::SPIRV_COLOR_FS));
    if (vs == VK_NULL_HANDLE || fs == VK_NULL_HANDLE) return false;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    // No vertex bindings; corners are baked into the shader.

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Source-alpha premultiplied-over, matching macOS / Windows setup.
    VkPipelineColorBlendAttachmentState cba{};
    cba.blendEnable = VK_TRUE;
    cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.colorBlendOp = VK_BLEND_OP_ADD;
    cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.alphaBlendOp = VK_BLEND_OP_ADD;
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                       | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    // Dynamic viewport / scissor so the same pipeline survives
    // swapchain resizes without recreation.
    VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    ds.dynamicStateCount = 2;
    ds.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo gci{};
    gci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gci.stageCount = 2;
    gci.pStages = stages;
    gci.pVertexInputState = &vi;
    gci.pInputAssemblyState = &ia;
    gci.pViewportState = &vp;
    gci.pRasterizationState = &rs;
    gci.pMultisampleState = &ms;
    gci.pColorBlendState = &cb;
    gci.pDynamicState = &ds;
    gci.layout = g_renderer.color_pipeline_layout;
    gci.renderPass = g_renderer.render_pass;
    gci.subpass = 0;

    bool ok = vk_ok(vkCreateGraphicsPipelines(
                        g_renderer.device, VK_NULL_HANDLE, 1, &gci, nullptr,
                        &g_renderer.color_pipeline),
                   "vkCreateGraphicsPipelines");

    vkDestroyShaderModule(g_renderer.device, vs, nullptr);
    vkDestroyShaderModule(g_renderer.device, fs, nullptr);
    return ok;
}

inline bool create_descriptor_pool_and_set() {
    if (g_renderer.color_descriptor_set != VK_NULL_HANDLE) return true;

    VkDescriptorPoolSize sizes[2]{};
    sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[0].descriptorCount = 1;
    sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    sizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.maxSets = 1;
    pci.poolSizeCount = 2;
    pci.pPoolSizes = sizes;
    if (!vk_ok(vkCreateDescriptorPool(g_renderer.device, &pci, nullptr,
                                      &g_renderer.color_descriptor_pool),
              "vkCreateDescriptorPool"))
        return false;

    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = g_renderer.color_descriptor_pool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &g_renderer.color_descriptor_set_layout;
    if (!vk_ok(vkAllocateDescriptorSets(g_renderer.device, &ai,
                                        &g_renderer.color_descriptor_set),
              "vkAllocateDescriptorSets"))
        return false;
    write_color_descriptor_set();
    return true;
}

inline bool create_color_resources() {
    // Idempotent: first swapchain creation does the full build; later
    // swapchain recreates reuse render_pass / pipeline / buffers and
    // only rebuild the image views + framebuffers.
    if (!create_render_pass()) return false;
    if (!create_color_pipeline()) return false;

    if (g_renderer.uniform_buffer == VK_NULL_HANDLE) {
        if (!create_host_buffer(sizeof(ColorUniforms),
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                g_renderer.uniform_buffer,
                                g_renderer.uniform_memory,
                                g_renderer.uniform_mapped))
            return false;
    }
    if (g_renderer.instance_buffer == VK_NULL_HANDLE) {
        if (!create_host_buffer(
                INITIAL_INSTANCE_CAPACITY * sizeof(ColorInstanceGPU),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                g_renderer.instance_buffer,
                g_renderer.instance_memory,
                g_renderer.instance_mapped))
            return false;
        g_renderer.instance_capacity = INITIAL_INSTANCE_CAPACITY;
    }
    if (!create_descriptor_pool_and_set()) return false;
    return true;
}

// ---- Stage 4 text pipeline Vulkan helpers ----------------------------

inline constexpr std::size_t INITIAL_TEXT_INSTANCE_CAPACITY = 64;

inline bool create_text_sampler() {
    if (g_renderer.text_sampler != VK_NULL_HANDLE) return true;
    VkSamplerCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    ci.magFilter = VK_FILTER_LINEAR;
    ci.minFilter = VK_FILTER_LINEAR;
    ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.unnormalizedCoordinates = VK_FALSE;
    ci.minLod = 0.0f;
    ci.maxLod = 0.0f;
    ci.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    return vk_ok(vkCreateSampler(g_renderer.device, &ci, nullptr,
                                 &g_renderer.text_sampler),
                "vkCreateSampler");
}

inline bool create_text_pipeline() {
    if (g_renderer.text_pipeline != VK_NULL_HANDLE) return true;

    VkDescriptorSetLayoutBinding bindings[3]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dslci{};
    dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = 3;
    dslci.pBindings = bindings;
    if (!vk_ok(vkCreateDescriptorSetLayout(
                  g_renderer.device, &dslci, nullptr,
                  &g_renderer.text_descriptor_set_layout),
              "vkCreateDescriptorSetLayout(text)"))
        return false;

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &g_renderer.text_descriptor_set_layout;
    if (!vk_ok(vkCreatePipelineLayout(g_renderer.device, &plci, nullptr,
                                      &g_renderer.text_pipeline_layout),
              "vkCreatePipelineLayout(text)"))
        return false;

    auto vs = create_shader_module(shaders::SPIRV_TEXT_VS,
                                   sizeof(shaders::SPIRV_TEXT_VS));
    auto fs = create_shader_module(shaders::SPIRV_TEXT_FS,
                                   sizeof(shaders::SPIRV_TEXT_FS));
    if (vs == VK_NULL_HANDLE || fs == VK_NULL_HANDLE) return false;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cba{};
    cba.blendEnable = VK_TRUE;
    cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.colorBlendOp = VK_BLEND_OP_ADD;
    cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.alphaBlendOp = VK_BLEND_OP_ADD;
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                       | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    ds.dynamicStateCount = 2;
    ds.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo gci{};
    gci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gci.stageCount = 2;
    gci.pStages = stages;
    gci.pVertexInputState = &vi;
    gci.pInputAssemblyState = &ia;
    gci.pViewportState = &vp;
    gci.pRasterizationState = &rs;
    gci.pMultisampleState = &ms;
    gci.pColorBlendState = &cb;
    gci.pDynamicState = &ds;
    gci.layout = g_renderer.text_pipeline_layout;
    gci.renderPass = g_renderer.render_pass;
    gci.subpass = 0;

    bool ok = vk_ok(vkCreateGraphicsPipelines(
                        g_renderer.device, VK_NULL_HANDLE, 1, &gci, nullptr,
                        &g_renderer.text_pipeline),
                   "vkCreateGraphicsPipelines(text)");

    vkDestroyShaderModule(g_renderer.device, vs, nullptr);
    vkDestroyShaderModule(g_renderer.device, fs, nullptr);
    return ok;
}

inline bool create_text_descriptor_pool_and_set() {
    if (g_renderer.text_descriptor_set != VK_NULL_HANDLE) return true;

    VkDescriptorPoolSize sizes[3]{};
    sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[0].descriptorCount = 1;
    sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    sizes[1].descriptorCount = 1;
    sizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[2].descriptorCount = 1;

    VkDescriptorPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.maxSets = 1;
    pci.poolSizeCount = 3;
    pci.pPoolSizes = sizes;
    if (!vk_ok(vkCreateDescriptorPool(g_renderer.device, &pci, nullptr,
                                      &g_renderer.text_descriptor_pool),
              "vkCreateDescriptorPool(text)"))
        return false;

    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = g_renderer.text_descriptor_pool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &g_renderer.text_descriptor_set_layout;
    return vk_ok(vkAllocateDescriptorSets(g_renderer.device, &ai,
                                          &g_renderer.text_descriptor_set),
                "vkAllocateDescriptorSets(text)");
}

inline void destroy_text_atlas_image() {
    if (g_renderer.text_atlas_view != VK_NULL_HANDLE) {
        vkDestroyImageView(g_renderer.device, g_renderer.text_atlas_view,
                           nullptr);
        g_renderer.text_atlas_view = VK_NULL_HANDLE;
    }
    if (g_renderer.text_atlas_image != VK_NULL_HANDLE) {
        vkDestroyImage(g_renderer.device, g_renderer.text_atlas_image,
                       nullptr);
        g_renderer.text_atlas_image = VK_NULL_HANDLE;
    }
    if (g_renderer.text_atlas_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_renderer.device, g_renderer.text_atlas_memory,
                     nullptr);
        g_renderer.text_atlas_memory = VK_NULL_HANDLE;
    }
    g_renderer.text_atlas_width = 0;
    g_renderer.text_atlas_height = 0;
}

inline bool create_or_resize_text_atlas_image(std::uint32_t w,
                                              std::uint32_t h) {
    if (w == 0 || h == 0) return true;
    if (w == g_renderer.text_atlas_width
        && h == g_renderer.text_atlas_height
        && g_renderer.text_atlas_image != VK_NULL_HANDLE) {
        return true;
    }
    destroy_text_atlas_image();

    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_R8_UNORM;
    ici.extent = { w, h, 1 };
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!vk_ok(vkCreateImage(g_renderer.device, &ici, nullptr,
                             &g_renderer.text_atlas_image),
              "vkCreateImage(text_atlas)"))
        return false;

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(g_renderer.device,
                                 g_renderer.text_atlas_image, &req);
    auto mt = find_memory_type(req.memoryTypeBits,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!mt) return false;

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = *mt;
    if (!vk_ok(vkAllocateMemory(g_renderer.device, &mai, nullptr,
                                &g_renderer.text_atlas_memory),
              "vkAllocateMemory(text_atlas)"))
        return false;
    if (!vk_ok(vkBindImageMemory(g_renderer.device,
                                 g_renderer.text_atlas_image,
                                 g_renderer.text_atlas_memory, 0),
              "vkBindImageMemory(text_atlas)"))
        return false;

    VkImageViewCreateInfo vci{};
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image = g_renderer.text_atlas_image;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = VK_FORMAT_R8_UNORM;
    vci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    if (!vk_ok(vkCreateImageView(g_renderer.device, &vci, nullptr,
                                 &g_renderer.text_atlas_view),
              "vkCreateImageView(text_atlas)"))
        return false;

    g_renderer.text_atlas_width = w;
    g_renderer.text_atlas_height = h;
    return true;
}

inline void write_text_descriptor_set() {
    if (g_renderer.text_descriptor_set == VK_NULL_HANDLE) return;
    if (g_renderer.uniform_buffer == VK_NULL_HANDLE
        || g_renderer.text_instance_buffer == VK_NULL_HANDLE
        || g_renderer.text_atlas_view == VK_NULL_HANDLE
        || g_renderer.text_sampler == VK_NULL_HANDLE)
        return;

    VkDescriptorBufferInfo ubo{};
    ubo.buffer = g_renderer.uniform_buffer;
    ubo.offset = 0;
    ubo.range = sizeof(ColorUniforms);

    VkDescriptorBufferInfo ssbo{};
    ssbo.buffer = g_renderer.text_instance_buffer;
    ssbo.offset = 0;
    ssbo.range = VK_WHOLE_SIZE;

    VkDescriptorImageInfo img{};
    img.sampler = g_renderer.text_sampler;
    img.imageView = g_renderer.text_atlas_view;
    img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writes[3]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = g_renderer.text_descriptor_set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &ubo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = g_renderer.text_descriptor_set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &ssbo;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = g_renderer.text_descriptor_set;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].pImageInfo = &img;

    vkUpdateDescriptorSets(g_renderer.device, 3, writes, 0, nullptr);
}

inline bool ensure_text_instance_capacity(std::size_t required) {
    if (required <= g_renderer.text_instance_capacity) return true;
    std::size_t cap = g_renderer.text_instance_capacity == 0
                        ? INITIAL_TEXT_INSTANCE_CAPACITY
                        : g_renderer.text_instance_capacity;
    while (cap < required) cap *= 2;

    destroy_host_buffer(g_renderer.text_instance_buffer,
                        g_renderer.text_instance_memory,
                        g_renderer.text_instance_mapped);
    if (!create_host_buffer(cap * sizeof(TextInstanceGPU),
                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                            g_renderer.text_instance_buffer,
                            g_renderer.text_instance_memory,
                            g_renderer.text_instance_mapped))
        return false;
    g_renderer.text_instance_capacity = cap;
    write_text_descriptor_set();
    return true;
}

inline bool ensure_text_staging_capacity(VkDeviceSize bytes) {
    if (bytes <= g_renderer.text_staging_capacity
        && g_renderer.text_staging_buffer != VK_NULL_HANDLE)
        return true;
    destroy_host_buffer(g_renderer.text_staging_buffer,
                        g_renderer.text_staging_memory,
                        g_renderer.text_staging_mapped);
    VkDeviceSize cap = g_renderer.text_staging_capacity == 0
                           ? 4096
                           : g_renderer.text_staging_capacity;
    while (cap < bytes) cap *= 2;
    if (!create_host_buffer(cap, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            g_renderer.text_staging_buffer,
                            g_renderer.text_staging_memory,
                            g_renderer.text_staging_mapped))
        return false;
    g_renderer.text_staging_capacity = cap;
    return true;
}

inline bool create_text_resources() {
    if (!create_text_sampler()) return false;
    if (!create_text_pipeline()) return false;
    if (g_renderer.text_instance_buffer == VK_NULL_HANDLE) {
        if (!create_host_buffer(
                INITIAL_TEXT_INSTANCE_CAPACITY * sizeof(TextInstanceGPU),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                g_renderer.text_instance_buffer,
                g_renderer.text_instance_memory,
                g_renderer.text_instance_mapped))
            return false;
        g_renderer.text_instance_capacity = INITIAL_TEXT_INSTANCE_CAPACITY;
    }
    if (!create_text_descriptor_pool_and_set()) return false;
    // Placeholder 1x1 atlas so the descriptor set is valid before the
    // first real atlas upload. The sampler clamps to edge, so the
    // single sample never ends up inside a glyph quad (quads bind real
    // uv_rect values from build_atlas).
    if (g_renderer.text_atlas_image == VK_NULL_HANDLE) {
        if (!create_or_resize_text_atlas_image(1, 1)) return false;
    }
    write_text_descriptor_set();
    return true;
}

inline void record_text_atlas_upload(VkCommandBuffer cmd,
                                     unsigned char const* pixels,
                                     std::uint32_t w, std::uint32_t h) {
    VkImageMemoryBarrier to_transfer{};
    to_transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_transfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.image = g_renderer.text_atlas_image;
    to_transfer.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    to_transfer.srcAccessMask = 0;
    to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &to_transfer);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { w, h, 1 };
    std::memcpy(g_renderer.text_staging_mapped, pixels,
                static_cast<std::size_t>(w) * h);
    vkCmdCopyBufferToImage(cmd, g_renderer.text_staging_buffer,
                           g_renderer.text_atlas_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &region);

    VkImageMemoryBarrier to_shader{};
    to_shader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_shader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_shader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_shader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader.image = g_renderer.text_atlas_image;
    to_shader.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    to_shader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_shader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &to_shader);
}

inline void destroy_text_pipeline_resources() {
    if (g_renderer.text_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(g_renderer.device,
                                g_renderer.text_descriptor_pool, nullptr);
        g_renderer.text_descriptor_pool = VK_NULL_HANDLE;
        g_renderer.text_descriptor_set = VK_NULL_HANDLE;
    }
    destroy_host_buffer(g_renderer.text_staging_buffer,
                        g_renderer.text_staging_memory,
                        g_renderer.text_staging_mapped);
    g_renderer.text_staging_capacity = 0;
    destroy_host_buffer(g_renderer.text_instance_buffer,
                        g_renderer.text_instance_memory,
                        g_renderer.text_instance_mapped);
    g_renderer.text_instance_capacity = 0;
    destroy_text_atlas_image();
    if (g_renderer.text_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(g_renderer.device, g_renderer.text_sampler, nullptr);
        g_renderer.text_sampler = VK_NULL_HANDLE;
    }
    if (g_renderer.text_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_renderer.device, g_renderer.text_pipeline,
                          nullptr);
        g_renderer.text_pipeline = VK_NULL_HANDLE;
    }
    if (g_renderer.text_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(g_renderer.device,
                                g_renderer.text_pipeline_layout, nullptr);
        g_renderer.text_pipeline_layout = VK_NULL_HANDLE;
    }
    if (g_renderer.text_descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(g_renderer.device,
                                     g_renderer.text_descriptor_set_layout,
                                     nullptr);
        g_renderer.text_descriptor_set_layout = VK_NULL_HANDLE;
    }
}

// ---- Stage 5 image pipeline Vulkan helpers ---------------------------

inline constexpr std::size_t INITIAL_IMAGE_INSTANCE_CAPACITY = 32;

inline bool create_image_sampler() {
    if (g_renderer.image_sampler != VK_NULL_HANDLE) return true;
    VkSamplerCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    ci.magFilter = VK_FILTER_LINEAR;
    ci.minFilter = VK_FILTER_LINEAR;
    ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    ci.unnormalizedCoordinates = VK_FALSE;
    ci.minLod = 0.0f;
    ci.maxLod = 0.0f;
    ci.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    return vk_ok(vkCreateSampler(g_renderer.device, &ci, nullptr,
                                 &g_renderer.image_sampler),
                "vkCreateSampler(image)");
}

inline bool create_image_pipeline() {
    if (g_renderer.image_pipeline != VK_NULL_HANDLE) return true;

    VkDescriptorSetLayoutBinding bindings[3]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dslci{};
    dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = 3;
    dslci.pBindings = bindings;
    if (!vk_ok(vkCreateDescriptorSetLayout(
                  g_renderer.device, &dslci, nullptr,
                  &g_renderer.image_descriptor_set_layout),
              "vkCreateDescriptorSetLayout(image)"))
        return false;

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &g_renderer.image_descriptor_set_layout;
    if (!vk_ok(vkCreatePipelineLayout(g_renderer.device, &plci, nullptr,
                                      &g_renderer.image_pipeline_layout),
              "vkCreatePipelineLayout(image)"))
        return false;

    auto vs = create_shader_module(shaders::SPIRV_IMAGE_VS,
                                   sizeof(shaders::SPIRV_IMAGE_VS));
    auto fs = create_shader_module(shaders::SPIRV_IMAGE_FS,
                                   sizeof(shaders::SPIRV_IMAGE_FS));
    if (vs == VK_NULL_HANDLE || fs == VK_NULL_HANDLE) return false;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cba{};
    cba.blendEnable = VK_TRUE;
    cba.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cba.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.colorBlendOp = VK_BLEND_OP_ADD;
    cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cba.alphaBlendOp = VK_BLEND_OP_ADD;
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                       | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;

    VkDynamicState dyn[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    ds.dynamicStateCount = 2;
    ds.pDynamicStates = dyn;

    VkGraphicsPipelineCreateInfo gci{};
    gci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gci.stageCount = 2;
    gci.pStages = stages;
    gci.pVertexInputState = &vi;
    gci.pInputAssemblyState = &ia;
    gci.pViewportState = &vp;
    gci.pRasterizationState = &rs;
    gci.pMultisampleState = &ms;
    gci.pColorBlendState = &cb;
    gci.pDynamicState = &ds;
    gci.layout = g_renderer.image_pipeline_layout;
    gci.renderPass = g_renderer.render_pass;
    gci.subpass = 0;

    bool ok = vk_ok(vkCreateGraphicsPipelines(
                        g_renderer.device, VK_NULL_HANDLE, 1, &gci, nullptr,
                        &g_renderer.image_pipeline),
                   "vkCreateGraphicsPipelines(image)");
    vkDestroyShaderModule(g_renderer.device, vs, nullptr);
    vkDestroyShaderModule(g_renderer.device, fs, nullptr);
    return ok;
}

inline bool create_image_descriptor_pool_and_set() {
    if (g_renderer.image_descriptor_set != VK_NULL_HANDLE) return true;

    VkDescriptorPoolSize sizes[3]{};
    sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[0].descriptorCount = 1;
    sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    sizes[1].descriptorCount = 1;
    sizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[2].descriptorCount = 1;

    VkDescriptorPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pci.maxSets = 1;
    pci.poolSizeCount = 3;
    pci.pPoolSizes = sizes;
    if (!vk_ok(vkCreateDescriptorPool(g_renderer.device, &pci, nullptr,
                                      &g_renderer.image_descriptor_pool),
              "vkCreateDescriptorPool(image)"))
        return false;

    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = g_renderer.image_descriptor_pool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &g_renderer.image_descriptor_set_layout;
    return vk_ok(vkAllocateDescriptorSets(g_renderer.device, &ai,
                                          &g_renderer.image_descriptor_set),
                "vkAllocateDescriptorSets(image)");
}

inline bool create_image_atlas_image() {
    if (g_renderer.image_atlas_image != VK_NULL_HANDLE) return true;

    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_R8G8B8A8_UNORM;
    ici.extent = { ImageAtlas::SIZE, ImageAtlas::SIZE, 1 };
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!vk_ok(vkCreateImage(g_renderer.device, &ici, nullptr,
                             &g_renderer.image_atlas_image),
              "vkCreateImage(image_atlas)"))
        return false;

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(g_renderer.device,
                                 g_renderer.image_atlas_image, &req);
    auto mt = find_memory_type(req.memoryTypeBits,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!mt) return false;

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = *mt;
    if (!vk_ok(vkAllocateMemory(g_renderer.device, &mai, nullptr,
                                &g_renderer.image_atlas_memory),
              "vkAllocateMemory(image_atlas)"))
        return false;
    if (!vk_ok(vkBindImageMemory(g_renderer.device,
                                 g_renderer.image_atlas_image,
                                 g_renderer.image_atlas_memory, 0),
              "vkBindImageMemory(image_atlas)"))
        return false;

    VkImageViewCreateInfo vci{};
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image = g_renderer.image_atlas_image;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = VK_FORMAT_R8G8B8A8_UNORM;
    vci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    if (!vk_ok(vkCreateImageView(g_renderer.device, &vci, nullptr,
                                 &g_renderer.image_atlas_view),
              "vkCreateImageView(image_atlas)"))
        return false;
    g_renderer.image_atlas_initialised = false; // needs first upload
    return true;
}

inline void write_image_descriptor_set() {
    if (g_renderer.image_descriptor_set == VK_NULL_HANDLE) return;
    if (g_renderer.uniform_buffer == VK_NULL_HANDLE
        || g_renderer.image_instance_buffer == VK_NULL_HANDLE
        || g_renderer.image_atlas_view == VK_NULL_HANDLE
        || g_renderer.image_sampler == VK_NULL_HANDLE)
        return;

    VkDescriptorBufferInfo ubo{};
    ubo.buffer = g_renderer.uniform_buffer;
    ubo.offset = 0;
    ubo.range = sizeof(ColorUniforms);

    VkDescriptorBufferInfo ssbo{};
    ssbo.buffer = g_renderer.image_instance_buffer;
    ssbo.offset = 0;
    ssbo.range = VK_WHOLE_SIZE;

    VkDescriptorImageInfo img{};
    img.sampler = g_renderer.image_sampler;
    img.imageView = g_renderer.image_atlas_view;
    img.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet writes[3]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = g_renderer.image_descriptor_set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &ubo;
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = g_renderer.image_descriptor_set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &ssbo;
    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = g_renderer.image_descriptor_set;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].pImageInfo = &img;

    vkUpdateDescriptorSets(g_renderer.device, 3, writes, 0, nullptr);
}

inline bool ensure_image_instance_capacity(std::size_t required) {
    if (required <= g_renderer.image_instance_capacity) return true;
    std::size_t cap = g_renderer.image_instance_capacity == 0
                        ? INITIAL_IMAGE_INSTANCE_CAPACITY
                        : g_renderer.image_instance_capacity;
    while (cap < required) cap *= 2;

    destroy_host_buffer(g_renderer.image_instance_buffer,
                        g_renderer.image_instance_memory,
                        g_renderer.image_instance_mapped);
    if (!create_host_buffer(cap * sizeof(ImageInstanceGPU),
                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                            g_renderer.image_instance_buffer,
                            g_renderer.image_instance_memory,
                            g_renderer.image_instance_mapped))
        return false;
    g_renderer.image_instance_capacity = cap;
    write_image_descriptor_set();
    return true;
}

inline bool ensure_image_staging_capacity(VkDeviceSize bytes) {
    if (bytes <= g_renderer.image_staging_capacity
        && g_renderer.image_staging_buffer != VK_NULL_HANDLE)
        return true;
    destroy_host_buffer(g_renderer.image_staging_buffer,
                        g_renderer.image_staging_memory,
                        g_renderer.image_staging_mapped);
    VkDeviceSize cap = g_renderer.image_staging_capacity == 0
                           ? 64 * 1024
                           : g_renderer.image_staging_capacity;
    while (cap < bytes) cap *= 2;
    if (!create_host_buffer(cap, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            g_renderer.image_staging_buffer,
                            g_renderer.image_staging_memory,
                            g_renderer.image_staging_mapped))
        return false;
    g_renderer.image_staging_capacity = cap;
    return true;
}

inline bool create_image_resources() {
    if (!create_image_sampler()) return false;
    if (!create_image_pipeline()) return false;
    if (g_renderer.image_instance_buffer == VK_NULL_HANDLE) {
        if (!create_host_buffer(
                INITIAL_IMAGE_INSTANCE_CAPACITY * sizeof(ImageInstanceGPU),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                g_renderer.image_instance_buffer,
                g_renderer.image_instance_memory,
                g_renderer.image_instance_mapped))
            return false;
        g_renderer.image_instance_capacity = INITIAL_IMAGE_INSTANCE_CAPACITY;
    }
    if (!create_image_descriptor_pool_and_set()) return false;
    if (!create_image_atlas_image()) return false;
    write_image_descriptor_set();
    return true;
}

// Transitions the atlas image layout to TRANSFER_DST_OPTIMAL (from
// UNDEFINED on first use, from SHADER_READ_ONLY_OPTIMAL otherwise),
// records a buffer→image copy for the current dirty rect, and
// transitions back to SHADER_READ_ONLY_OPTIMAL so the next frame's
// fragment shader can sample it. Must be called outside any render
// pass.
inline void record_image_atlas_upload(VkCommandBuffer cmd) {
    if (!g_images.has_dirty) return;
    if (g_images.dirty_min_x >= g_images.dirty_max_x
        || g_images.dirty_min_y >= g_images.dirty_max_y) {
        g_images.has_dirty = false;
        return;
    }
    std::uint32_t const dx = g_images.dirty_min_x;
    std::uint32_t const dy = g_images.dirty_min_y;
    std::uint32_t const dw = g_images.dirty_max_x - g_images.dirty_min_x;
    std::uint32_t const dh = g_images.dirty_max_y - g_images.dirty_min_y;

    // Pack the dirty rect tightly into the staging buffer so the
    // upload isn't 16 MiB when only a few KB changed.
    VkDeviceSize const needed = static_cast<VkDeviceSize>(dw)
                              * dh * ImageAtlas::BYTES_PER_PIXEL;
    if (!ensure_image_staging_capacity(needed)) {
        g_images.has_dirty = false;
        return;
    }
    auto* dst = static_cast<std::uint8_t*>(g_renderer.image_staging_mapped);
    std::size_t const src_row =
        static_cast<std::size_t>(ImageAtlas::SIZE) * ImageAtlas::BYTES_PER_PIXEL;
    std::size_t const dst_row =
        static_cast<std::size_t>(dw) * ImageAtlas::BYTES_PER_PIXEL;
    for (std::uint32_t y = 0; y < dh; ++y) {
        auto const* src = g_images.pixels.data()
                        + static_cast<std::size_t>(dy + y) * src_row
                        + static_cast<std::size_t>(dx) * ImageAtlas::BYTES_PER_PIXEL;
        std::memcpy(dst + static_cast<std::size_t>(y) * dst_row, src, dst_row);
    }

    VkImageMemoryBarrier to_transfer{};
    to_transfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_transfer.oldLayout = g_renderer.image_atlas_initialised
                              ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                              : VK_IMAGE_LAYOUT_UNDEFINED;
    to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.image = g_renderer.image_atlas_image;
    to_transfer.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    to_transfer.srcAccessMask = g_renderer.image_atlas_initialised
                                  ? VK_ACCESS_SHADER_READ_BIT
                                  : 0;
    to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
        g_renderer.image_atlas_initialised
            ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
            : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &to_transfer);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = dw;     // tightly packed inside staging
    region.bufferImageHeight = dh;
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageOffset = { static_cast<std::int32_t>(dx),
                           static_cast<std::int32_t>(dy), 0 };
    region.imageExtent = { dw, dh, 1 };
    vkCmdCopyBufferToImage(cmd, g_renderer.image_staging_buffer,
                           g_renderer.image_atlas_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &region);

    VkImageMemoryBarrier to_shader{};
    to_shader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_shader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_shader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_shader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader.image = g_renderer.image_atlas_image;
    to_shader.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    to_shader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_shader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &to_shader);

    g_renderer.image_atlas_initialised = true;
    g_images.has_dirty = false;
    g_images.dirty_min_x = ImageAtlas::SIZE;
    g_images.dirty_min_y = ImageAtlas::SIZE;
    g_images.dirty_max_x = 0;
    g_images.dirty_max_y = 0;
}

inline void destroy_image_pipeline_resources() {
    if (g_renderer.image_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(g_renderer.device,
                                g_renderer.image_descriptor_pool, nullptr);
        g_renderer.image_descriptor_pool = VK_NULL_HANDLE;
        g_renderer.image_descriptor_set = VK_NULL_HANDLE;
    }
    destroy_host_buffer(g_renderer.image_staging_buffer,
                        g_renderer.image_staging_memory,
                        g_renderer.image_staging_mapped);
    g_renderer.image_staging_capacity = 0;
    destroy_host_buffer(g_renderer.image_instance_buffer,
                        g_renderer.image_instance_memory,
                        g_renderer.image_instance_mapped);
    g_renderer.image_instance_capacity = 0;
    if (g_renderer.image_atlas_view != VK_NULL_HANDLE) {
        vkDestroyImageView(g_renderer.device, g_renderer.image_atlas_view,
                           nullptr);
        g_renderer.image_atlas_view = VK_NULL_HANDLE;
    }
    if (g_renderer.image_atlas_image != VK_NULL_HANDLE) {
        vkDestroyImage(g_renderer.device, g_renderer.image_atlas_image,
                       nullptr);
        g_renderer.image_atlas_image = VK_NULL_HANDLE;
    }
    if (g_renderer.image_atlas_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_renderer.device, g_renderer.image_atlas_memory,
                     nullptr);
        g_renderer.image_atlas_memory = VK_NULL_HANDLE;
    }
    g_renderer.image_atlas_initialised = false;
    if (g_renderer.image_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(g_renderer.device, g_renderer.image_sampler, nullptr);
        g_renderer.image_sampler = VK_NULL_HANDLE;
    }
    if (g_renderer.image_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_renderer.device, g_renderer.image_pipeline,
                          nullptr);
        g_renderer.image_pipeline = VK_NULL_HANDLE;
    }
    if (g_renderer.image_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(g_renderer.device,
                                g_renderer.image_pipeline_layout, nullptr);
        g_renderer.image_pipeline_layout = VK_NULL_HANDLE;
    }
    if (g_renderer.image_descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(g_renderer.device,
                                     g_renderer.image_descriptor_set_layout,
                                     nullptr);
        g_renderer.image_descriptor_set_layout = VK_NULL_HANDLE;
    }
    // The CPU-side atlas survives shutdown so repeat attach_surface
    // calls don't re-decode cached images; cache is drained on
    // phenotype_android_detach_surface via a separate clear helper.
}

inline void reset_image_cache() {
    g_images.cache.clear();
    g_images.cursor_x = 0;
    g_images.cursor_y = 0;
    g_images.row_height = 0;
    g_images.dirty_min_x = ImageAtlas::SIZE;
    g_images.dirty_min_y = ImageAtlas::SIZE;
    g_images.dirty_max_x = 0;
    g_images.dirty_max_y = 0;
    g_images.has_dirty = false;
    g_images.pixels.clear();
    g_images.pixels.shrink_to_fit();
}

// ---- Stage 7 debug-capture image -------------------------------------
//
// Persistent RGBA8 copy target sized to the current swapchain extent.
// Created alongside the swapchain (because extent drives the size) and
// destroyed in destroy_swapchain_resources. Every renderer_flush copies
// the just-presented swapchain image into this target after the render
// pass ends; capture_frame_rgba() reads the target on demand via a
// staging buffer.

inline void destroy_debug_capture_image() {
    if (g_renderer.debug_capture_view != VK_NULL_HANDLE) {
        vkDestroyImageView(g_renderer.device, g_renderer.debug_capture_view,
                           nullptr);
        g_renderer.debug_capture_view = VK_NULL_HANDLE;
    }
    if (g_renderer.debug_capture_image != VK_NULL_HANDLE) {
        vkDestroyImage(g_renderer.device, g_renderer.debug_capture_image,
                       nullptr);
        g_renderer.debug_capture_image = VK_NULL_HANDLE;
    }
    if (g_renderer.debug_capture_memory != VK_NULL_HANDLE) {
        vkFreeMemory(g_renderer.device, g_renderer.debug_capture_memory,
                     nullptr);
        g_renderer.debug_capture_memory = VK_NULL_HANDLE;
    }
    g_renderer.debug_capture_ready = false;
}

inline bool create_debug_capture_image() {
    destroy_debug_capture_image();
    auto const& ext = g_renderer.swapchain_extent;
    if (ext.width == 0 || ext.height == 0) return true;

    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    // Match the swapchain format so vkCmdCopyImage is valid (no
    // conversion required). swapchain_format is set in create_swapchain
    // to VK_FORMAT_R8G8B8A8_UNORM when available.
    ici.format = g_renderer.swapchain_format;
    ici.extent = { ext.width, ext.height, 1 };
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
              | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!vk_ok(vkCreateImage(g_renderer.device, &ici, nullptr,
                             &g_renderer.debug_capture_image),
              "vkCreateImage(debug_capture)"))
        return false;

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(g_renderer.device,
                                 g_renderer.debug_capture_image, &req);
    auto mt = find_memory_type(req.memoryTypeBits,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!mt) return false;

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = *mt;
    if (!vk_ok(vkAllocateMemory(g_renderer.device, &mai, nullptr,
                                &g_renderer.debug_capture_memory),
              "vkAllocateMemory(debug_capture)"))
        return false;
    if (!vk_ok(vkBindImageMemory(g_renderer.device,
                                 g_renderer.debug_capture_image,
                                 g_renderer.debug_capture_memory, 0),
              "vkBindImageMemory(debug_capture)"))
        return false;

    VkImageViewCreateInfo vci{};
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image = g_renderer.debug_capture_image;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = g_renderer.swapchain_format;
    vci.components = { VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY,
                       VK_COMPONENT_SWIZZLE_IDENTITY };
    vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    return vk_ok(vkCreateImageView(g_renderer.device, &vci, nullptr,
                                   &g_renderer.debug_capture_view),
                "vkCreateImageView(debug_capture)");
}

inline bool ensure_debug_readback_capacity(VkDeviceSize bytes) {
    if (bytes <= g_renderer.debug_readback_capacity
        && g_renderer.debug_readback_buffer != VK_NULL_HANDLE)
        return true;
    destroy_host_buffer(g_renderer.debug_readback_buffer,
                        g_renderer.debug_readback_memory,
                        g_renderer.debug_readback_mapped);
    VkDeviceSize cap = g_renderer.debug_readback_capacity == 0
                           ? 1024 * 1024 // 1 MiB starter
                           : g_renderer.debug_readback_capacity;
    while (cap < bytes) cap *= 2;
    if (!create_host_buffer(cap, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                            g_renderer.debug_readback_buffer,
                            g_renderer.debug_readback_memory,
                            g_renderer.debug_readback_mapped))
        return false;
    g_renderer.debug_readback_capacity = cap;
    return true;
}

inline void destroy_debug_readback_buffer() {
    destroy_host_buffer(g_renderer.debug_readback_buffer,
                        g_renderer.debug_readback_memory,
                        g_renderer.debug_readback_mapped);
    g_renderer.debug_readback_capacity = 0;
}

// Records the per-frame copy from the just-presented swapchain image
// into debug_capture_image. Must be called after vkCmdEndRenderPass
// and before vkEndCommandBuffer. Leaves the swapchain image back in
// PRESENT_SRC_KHR and debug_capture_image in GENERAL so the capture
// submit path can transition it to TRANSFER_SRC on demand.
inline void record_debug_capture_copy(VkCommandBuffer cmd,
                                      std::uint32_t swapchain_index) {
    if (g_renderer.debug_capture_image == VK_NULL_HANDLE) return;
    auto const& ext = g_renderer.swapchain_extent;
    if (ext.width == 0 || ext.height == 0) return;

    VkImageMemoryBarrier pre[2]{};
    // swapchain PRESENT_SRC -> TRANSFER_SRC
    pre[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    pre[0].oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    pre[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    pre[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    pre[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    pre[0].image = g_renderer.swapchain_images[swapchain_index];
    pre[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    pre[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    pre[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    // capture GENERAL/UNDEFINED -> TRANSFER_DST
    pre[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    pre[1].oldLayout = g_renderer.debug_capture_ready
                        ? VK_IMAGE_LAYOUT_GENERAL
                        : VK_IMAGE_LAYOUT_UNDEFINED;
    pre[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    pre[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    pre[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    pre[1].image = g_renderer.debug_capture_image;
    pre[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    pre[1].srcAccessMask = g_renderer.debug_capture_ready
                            ? VK_ACCESS_TRANSFER_READ_BIT
                            : 0;
    pre[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 2, pre);

    VkImageCopy region{};
    region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.extent = { ext.width, ext.height, 1 };
    vkCmdCopyImage(cmd,
        g_renderer.swapchain_images[swapchain_index],
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        g_renderer.debug_capture_image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region);

    VkImageMemoryBarrier post[2]{};
    // swapchain TRANSFER_SRC -> PRESENT_SRC
    post[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    post[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    post[0].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    post[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    post[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    post[0].image = g_renderer.swapchain_images[swapchain_index];
    post[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    post[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    post[0].dstAccessMask = 0;
    // capture TRANSFER_DST -> GENERAL (read-ready)
    post[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    post[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    post[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
    post[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    post[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    post[1].image = g_renderer.debug_capture_image;
    post[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    post[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    post[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 2, post);

    g_renderer.debug_capture_ready = true;
    g_renderer.last_render_width  = ext.width;
    g_renderer.last_render_height = ext.height;
    g_renderer.last_frame_available = true;
}

inline bool create_image_views_and_framebuffers() {
    g_renderer.swapchain_image_views.resize(g_renderer.swapchain_images.size());
    for (std::size_t i = 0; i < g_renderer.swapchain_images.size(); ++i) {
        VkImageViewCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image = g_renderer.swapchain_images[i];
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = g_renderer.swapchain_format;
        ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                          VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        ci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        if (!vk_ok(vkCreateImageView(g_renderer.device, &ci, nullptr,
                                     &g_renderer.swapchain_image_views[i]),
                  "vkCreateImageView"))
            return false;
    }

    g_renderer.framebuffers.resize(g_renderer.swapchain_images.size());
    for (std::size_t i = 0; i < g_renderer.swapchain_images.size(); ++i) {
        VkFramebufferCreateInfo fi{};
        fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fi.renderPass = g_renderer.render_pass;
        fi.attachmentCount = 1;
        fi.pAttachments = &g_renderer.swapchain_image_views[i];
        fi.width = g_renderer.swapchain_extent.width;
        fi.height = g_renderer.swapchain_extent.height;
        fi.layers = 1;
        if (!vk_ok(vkCreateFramebuffer(g_renderer.device, &fi, nullptr,
                                       &g_renderer.framebuffers[i]),
                  "vkCreateFramebuffer"))
            return false;
    }
    return true;
}

inline void destroy_swapchain_resources() {
    if (g_renderer.device == VK_NULL_HANDLE) return;
    if (g_renderer.in_flight != VK_NULL_HANDLE)
        vkWaitForFences(g_renderer.device, 1, &g_renderer.in_flight, VK_TRUE, UINT64_MAX);

    // Stage 7: the debug capture image is sized to the current
    // swapchain extent, so its lifetime follows the swapchain.
    destroy_debug_capture_image();

    for (auto fb : g_renderer.framebuffers) {
        if (fb != VK_NULL_HANDLE)
            vkDestroyFramebuffer(g_renderer.device, fb, nullptr);
    }
    g_renderer.framebuffers.clear();

    for (auto iv : g_renderer.swapchain_image_views) {
        if (iv != VK_NULL_HANDLE)
            vkDestroyImageView(g_renderer.device, iv, nullptr);
    }
    g_renderer.swapchain_image_views.clear();

    if (g_renderer.command_pool != VK_NULL_HANDLE
        && g_renderer.command_buffer != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(g_renderer.device, g_renderer.command_pool,
                             1, &g_renderer.command_buffer);
        g_renderer.command_buffer = VK_NULL_HANDLE;
    }
    if (g_renderer.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(g_renderer.device, g_renderer.swapchain, nullptr);
        g_renderer.swapchain = VK_NULL_HANDLE;
    }
    g_renderer.swapchain_images.clear();
    g_renderer.last_frame_available = false;
}

inline bool create_swapchain() {
    VkSurfaceCapabilitiesKHR caps{};
    if (!vk_ok(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                  g_renderer.physical_device, g_renderer.surface, &caps),
              "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"))
        return false;

    std::uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        g_renderer.physical_device, g_renderer.surface, &format_count, nullptr);
    if (format_count == 0) return false;
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        g_renderer.physical_device, g_renderer.surface, &format_count, formats.data());

    VkSurfaceFormatKHR picked = formats[0];
    for (auto const& f : formats) {
        if (f.format == VK_FORMAT_R8G8B8A8_UNORM
            && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            picked = f;
            break;
        }
    }

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = static_cast<std::uint32_t>(
            ANativeWindow_getWidth(g_renderer.window));
        extent.height = static_cast<std::uint32_t>(
            ANativeWindow_getHeight(g_renderer.window));
    }

    std::uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount)
        image_count = caps.maxImageCount;

    VkSwapchainCreateInfoKHR ci{};
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = g_renderer.surface;
    ci.minImageCount = image_count;
    ci.imageFormat = picked.format;
    ci.imageColorSpace = picked.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    // Stage 3 renders through the color_pipeline render pass; the
    // swapchain images only need the COLOR_ATTACHMENT usage.
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    ci.clipped = VK_TRUE;

    if (!vk_ok(vkCreateSwapchainKHR(g_renderer.device, &ci, nullptr,
                                    &g_renderer.swapchain),
              "vkCreateSwapchainKHR"))
        return false;

    g_renderer.swapchain_format = picked.format;
    g_renderer.swapchain_extent = extent;

    std::uint32_t n = 0;
    vkGetSwapchainImagesKHR(g_renderer.device, g_renderer.swapchain, &n, nullptr);
    g_renderer.swapchain_images.resize(n);
    vkGetSwapchainImagesKHR(g_renderer.device, g_renderer.swapchain, &n,
                            g_renderer.swapchain_images.data());

    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = g_renderer.command_pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    if (!vk_ok(vkAllocateCommandBuffers(g_renderer.device, &alloc,
                                        &g_renderer.command_buffer),
              "vkAllocateCommandBuffers"))
        return false;

    if (!create_color_resources()) return false;
    if (!create_text_resources()) return false;
    if (!create_image_resources()) return false;
    if (!create_image_views_and_framebuffers()) return false;
    if (!create_debug_capture_image()) return false;
    return true;
}

inline void renderer_init(native_surface_handle handle) {
    auto* window = static_cast<ANativeWindow*>(handle);
    if (g_renderer.initialized && g_renderer.window == window) return;
    if (g_renderer.initialized) destroy_swapchain_resources();
    g_renderer.window = window;
    if (!window) return;

    if (g_renderer.instance == VK_NULL_HANDLE) {
        VkApplicationInfo app{};
        app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app.pApplicationName = "phenotype";
        app.apiVersion = VK_API_VERSION_1_1;
        char const* exts[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
        };
        VkInstanceCreateInfo ici{};
        ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ici.pApplicationInfo = &app;
        ici.enabledExtensionCount = 2;
        ici.ppEnabledExtensionNames = exts;
        if (!vk_ok(vkCreateInstance(&ici, nullptr, &g_renderer.instance),
                  "vkCreateInstance"))
            return;
    }

    if (g_renderer.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_renderer.instance, g_renderer.surface, nullptr);
        g_renderer.surface = VK_NULL_HANDLE;
    }

    VkAndroidSurfaceCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    sci.window = window;
    if (!vk_ok(vkCreateAndroidSurfaceKHR(g_renderer.instance, &sci, nullptr,
                                         &g_renderer.surface),
              "vkCreateAndroidSurfaceKHR"))
        return;

    if (g_renderer.device == VK_NULL_HANDLE) {
        std::uint32_t n = 0;
        vkEnumeratePhysicalDevices(g_renderer.instance, &n, nullptr);
        if (n == 0) return;
        std::vector<VkPhysicalDevice> devs(n);
        vkEnumeratePhysicalDevices(g_renderer.instance, &n, devs.data());
        g_renderer.physical_device = devs[0];

        std::uint32_t q = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(g_renderer.physical_device, &q, nullptr);
        std::vector<VkQueueFamilyProperties> fams(q);
        vkGetPhysicalDeviceQueueFamilyProperties(
            g_renderer.physical_device, &q, fams.data());
        bool found = false;
        for (std::uint32_t i = 0; i < q; ++i) {
            if (fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                VkBool32 present = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(
                    g_renderer.physical_device, i, g_renderer.surface, &present);
                if (present) {
                    g_renderer.queue_family_index = i;
                    found = true;
                    break;
                }
            }
        }
        if (!found) return;

        float priority = 1.0f;
        VkDeviceQueueCreateInfo qci{};
        qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = g_renderer.queue_family_index;
        qci.queueCount = 1;
        qci.pQueuePriorities = &priority;

        char const* dev_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        VkDeviceCreateInfo dci{};
        dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        dci.queueCreateInfoCount = 1;
        dci.pQueueCreateInfos = &qci;
        dci.enabledExtensionCount = 1;
        dci.ppEnabledExtensionNames = dev_exts;
        if (!vk_ok(vkCreateDevice(g_renderer.physical_device, &dci, nullptr,
                                  &g_renderer.device),
                  "vkCreateDevice"))
            return;
        vkGetDeviceQueue(g_renderer.device, g_renderer.queue_family_index, 0,
                         &g_renderer.queue);

        VkCommandPoolCreateInfo pci{};
        pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pci.queueFamilyIndex = g_renderer.queue_family_index;
        vk_ok(vkCreateCommandPool(g_renderer.device, &pci, nullptr,
                                  &g_renderer.command_pool),
              "vkCreateCommandPool");

        VkSemaphoreCreateInfo semi{};
        semi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vk_ok(vkCreateSemaphore(g_renderer.device, &semi, nullptr,
                                &g_renderer.image_available),
              "vkCreateSemaphore");
        vk_ok(vkCreateSemaphore(g_renderer.device, &semi, nullptr,
                                &g_renderer.render_finished),
              "vkCreateSemaphore");

        VkFenceCreateInfo fi{};
        fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vk_ok(vkCreateFence(g_renderer.device, &fi, nullptr, &g_renderer.in_flight),
              "vkCreateFence");
    }

    if (!create_swapchain()) return;
    g_renderer.initialized = true;
    __android_log_print(ANDROID_LOG_INFO, ANDROID_LOG_TAG,
                        "Vulkan renderer ready (%ux%u)",
                        g_renderer.swapchain_extent.width,
                        g_renderer.swapchain_extent.height);
}

// Normalises the 0..255 Color channels into the 0..1 RGBA linear
// vec4 the GLSL shader expects.
inline void normalize_color(::phenotype::Color const& c, float out[4]) {
    out[0] = static_cast<float>(c.r) / 255.0f;
    out[1] = static_cast<float>(c.g) / 255.0f;
    out[2] = static_cast<float>(c.b) / 255.0f;
    out[3] = static_cast<float>(c.a) / 255.0f;
}

inline void decode_android_color_commands(unsigned char const* buf,
                                          unsigned int len,
                                          FrameScratch& out) {
    auto commands = ::phenotype::parse_commands(buf, len);
    for (auto const& cmd : commands) {
        std::visit([&](auto const& c) {
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::same_as<T, ::phenotype::ClearCmd>) {
                normalize_color(c.color, out.clear_color);
                out.has_clear = true;
            } else if constexpr (std::same_as<T, ::phenotype::FillRectCmd>) {
                ColorInstanceGPU inst{};
                inst.rect[0] = c.x; inst.rect[1] = c.y;
                inst.rect[2] = c.w; inst.rect[3] = c.h;
                normalize_color(c.color, inst.color);
                // params = (0, 0, 0=Fill, 0)
                out.color_instances.push_back(inst);
            } else if constexpr (std::same_as<T, ::phenotype::StrokeRectCmd>) {
                ColorInstanceGPU inst{};
                inst.rect[0] = c.x; inst.rect[1] = c.y;
                inst.rect[2] = c.w; inst.rect[3] = c.h;
                normalize_color(c.color, inst.color);
                inst.params[0] = 0.0f;
                inst.params[1] = c.line_width;
                inst.params[2] = 1.0f; // draw_type = Stroke
                out.color_instances.push_back(inst);
            } else if constexpr (std::same_as<T, ::phenotype::RoundRectCmd>) {
                ColorInstanceGPU inst{};
                inst.rect[0] = c.x; inst.rect[1] = c.y;
                inst.rect[2] = c.w; inst.rect[3] = c.h;
                normalize_color(c.color, inst.color);
                inst.params[0] = c.radius;
                inst.params[1] = 0.0f;
                inst.params[2] = 2.0f; // draw_type = Round
                out.color_instances.push_back(inst);
            } else if constexpr (std::same_as<T, ::phenotype::DrawLineCmd>) {
                // Thickened axis-aligned rect — preserves desktop
                // backends' DrawLine behavior including the
                // draw_type = 3 fall-through to the Fill code path.
                float dx = c.x2 - c.x1;
                float dy = c.y2 - c.y1;
                float line_len = std::sqrt(dx * dx + dy * dy);
                float w = (dy == 0.0f) ? line_len : c.thickness;
                float h = (dx == 0.0f) ? line_len : c.thickness;
                float x = (dx == 0.0f)
                    ? c.x1 - c.thickness * 0.5f
                    : std::min(c.x1, c.x2);
                float y = (dy == 0.0f)
                    ? c.y1 - c.thickness * 0.5f
                    : std::min(c.y1, c.y2);
                ColorInstanceGPU inst{};
                inst.rect[0] = x; inst.rect[1] = y;
                inst.rect[2] = w; inst.rect[3] = h;
                normalize_color(c.color, inst.color);
                inst.params[0] = 0.0f;
                inst.params[1] = 0.0f;
                inst.params[2] = 3.0f; // matches macOS / Windows
                out.color_instances.push_back(inst);
            } else if constexpr (std::same_as<T, ::phenotype::DrawTextCmd>) {
                ::phenotype::native::TextEntry entry{};
                entry.x = c.x;
                entry.y = c.y;
                entry.font_size = c.font_size;
                entry.mono = c.mono;
                entry.r = c.color.r / 255.0f;
                entry.g = c.color.g / 255.0f;
                entry.b = c.color.b / 255.0f;
                entry.a = c.color.a / 255.0f;
                entry.text = c.text;
                out.text_entries.push_back(std::move(entry));
            } else if constexpr (std::same_as<T, ::phenotype::DrawImageCmd>) {
                auto const* entry = ensure_image_cache_entry(c.url);
                if (entry && entry->state == ImageState::Ready) {
                    ImageInstanceGPU inst{};
                    inst.rect[0] = c.x; inst.rect[1] = c.y;
                    inst.rect[2] = c.w; inst.rect[3] = c.h;
                    inst.uv_rect[0] = entry->u;
                    inst.uv_rect[1] = entry->v;
                    inst.uv_rect[2] = entry->uw;
                    inst.uv_rect[3] = entry->vh;
                    inst.color[0] = 1.0f; inst.color[1] = 1.0f;
                    inst.color[2] = 1.0f; inst.color[3] = 1.0f;
                    out.image_instances.push_back(inst);
                } else {
                    // Placeholder: light-gray fill + darker border,
                    // matching macOS's image pending/failed rendering.
                    bool failed = entry && entry->state == ImageState::Failed;
                    float fill  = failed ? 0.90f : 0.94f;
                    float edge  = failed ? 0.78f : 0.82f;
                    ColorInstanceGPU inner{};
                    inner.rect[0] = c.x; inner.rect[1] = c.y;
                    inner.rect[2] = c.w; inner.rect[3] = c.h;
                    inner.color[0] = fill; inner.color[1] = fill;
                    inner.color[2] = fill; inner.color[3] = 1.0f;
                    // params = (0,0,0,0) → plain fill
                    out.color_instances.push_back(inner);
                    ColorInstanceGPU outline{};
                    outline.rect[0] = c.x; outline.rect[1] = c.y;
                    outline.rect[2] = c.w; outline.rect[3] = c.h;
                    outline.color[0] = edge; outline.color[1] = edge;
                    outline.color[2] = edge; outline.color[3] = 1.0f;
                    outline.params[0] = 0.0f;
                    outline.params[1] = 2.0f; // 2-pixel stroke
                    outline.params[2] = 1.0f; // draw_type = Stroke
                    out.color_instances.push_back(outline);
                }
            }
            // HitRegion is ignored before Stage 6.
        }, cmd);
    }
}

// Stage 5 demo composition: Stage 3's five color primitives + Stage 4's
// two DrawText labels + one DrawImage against the APK-bundled
// `stage5-demo.png` asset. Exercises the decoder + atlas build +
// image-pipeline render path end-to-end through the public
// phenotype::emit_* API. Stage 6 replaces this with a real View /
// Update loop bound to the example driver's State.
// Stage 6: the static build_stage5_demo path is retired. The render
// loop is now driven by phenotype's view/update via `repaint_current`,
// which lands a real command buffer in `host.buffer` before the shell
// calls our renderer_flush(buf, len). The demo6::view function below
// fills in for the static scene and pulls in color + text + image all
// through the public `phenotype::widget::*` / `phenotype::layout::*`
// API.

inline void renderer_flush(unsigned char const* buf, unsigned int len) {
    if (!g_renderer.initialized) return;
    if (buf == nullptr || len == 0) {
        g_renderer.last_frame_buf.clear();
        return;
    }

    // Snapshot the caller's buffer so hit_test can replay it later
    // without forcing another view() pass.
    g_renderer.last_frame_buf.assign(buf, buf + len);

    FrameScratch scratch;
    decode_android_color_commands(buf, len, scratch);
    if (!scratch.has_clear) {
        auto const& theme = ::phenotype::current_theme();
        normalize_color(theme.background, scratch.clear_color);
    }

    vkWaitForFences(g_renderer.device, 1, &g_renderer.in_flight, VK_TRUE, UINT64_MAX);

    // Build the text atlas on CPU (JNI) before acquiring the swapchain
    // image so the Paint calls can take as long as they need without
    // stalling image presentation. The previous frame's fence has been
    // waited on, so the persistently-mapped text buffers are safe to
    // touch once we start staging data below.
    ::phenotype::native::TextAtlas atlas{};
    if (!scratch.text_entries.empty()) {
        atlas = text_build_atlas(scratch.text_entries, 1.0f);
        scratch.text_instances.reserve(scratch.text_entries.size());
        for (std::size_t i = 0;
             i < scratch.text_entries.size() && i < atlas.quads.size();
             ++i) {
            auto const& entry = scratch.text_entries[i];
            auto const& q = atlas.quads[i];
            if (q.w <= 0.0f || q.h <= 0.0f) continue; // slot skipped
            TextInstanceGPU inst{};
            inst.rect[0] = q.x; inst.rect[1] = q.y;
            inst.rect[2] = q.w; inst.rect[3] = q.h;
            inst.uv_rect[0] = q.u;  inst.uv_rect[1] = q.v;
            inst.uv_rect[2] = q.uw; inst.uv_rect[3] = q.vh;
            inst.color[0] = entry.r; inst.color[1] = entry.g;
            inst.color[2] = entry.b; inst.color[3] = entry.a;
            scratch.text_instances.push_back(inst);
        }
    }

    std::uint32_t idx = 0;
    auto r = vkAcquireNextImageKHR(
        g_renderer.device, g_renderer.swapchain, UINT64_MAX,
        g_renderer.image_available, VK_NULL_HANDLE, &idx);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) {
        destroy_swapchain_resources();
        create_swapchain();
        return;
    }
    if (!vk_ok(r, "vkAcquireNextImageKHR")) return;

    vkResetFences(g_renderer.device, 1, &g_renderer.in_flight);

    // Upload per-frame data — the fence wait above guarantees the
    // previous frame is no longer reading these persistently-mapped
    // buffers.
    if (!scratch.color_instances.empty()) {
        if (!ensure_instance_capacity(scratch.color_instances.size())) return;
        std::memcpy(g_renderer.instance_mapped,
                    scratch.color_instances.data(),
                    scratch.color_instances.size() * sizeof(ColorInstanceGPU));
    }

    bool have_text = !scratch.text_instances.empty()
                   && !atlas.pixels.empty()
                   && atlas.width > 0 && atlas.height > 0;
    if (have_text) {
        if (!ensure_text_instance_capacity(scratch.text_instances.size())
            || !ensure_text_staging_capacity(
                   static_cast<VkDeviceSize>(atlas.pixels.size()))
            || !create_or_resize_text_atlas_image(
                   static_cast<std::uint32_t>(atlas.width),
                   static_cast<std::uint32_t>(atlas.height))) {
            have_text = false;
        } else {
            std::memcpy(g_renderer.text_instance_mapped,
                        scratch.text_instances.data(),
                        scratch.text_instances.size() * sizeof(TextInstanceGPU));
            // Atlas view may have been recreated; refresh all three
            // descriptor bindings so the fragment shader sees the
            // current VkImage + sampler pair.
            write_text_descriptor_set();
        }
    }

    // Image pipeline: upload the instance SSBO (atlas pixels already
    // live in g_images.pixels; record_image_atlas_upload below flushes
    // any dirty region to the VkImage).
    bool have_images = !scratch.image_instances.empty();
    if (have_images) {
        if (!ensure_image_instance_capacity(scratch.image_instances.size())) {
            have_images = false;
        } else {
            std::memcpy(g_renderer.image_instance_mapped,
                        scratch.image_instances.data(),
                        scratch.image_instances.size()
                            * sizeof(ImageInstanceGPU));
        }
    }

    ColorUniforms uniforms{};
    uniforms.viewport[0] = static_cast<float>(g_renderer.swapchain_extent.width);
    uniforms.viewport[1] = static_cast<float>(g_renderer.swapchain_extent.height);
    std::memcpy(g_renderer.uniform_mapped, &uniforms, sizeof uniforms);

    // Record the render pass.
    vkResetCommandBuffer(g_renderer.command_buffer, 0);
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(g_renderer.command_buffer, &bi);

    // Atlas staging → VkImage copy must happen outside the render pass;
    // layout transitions aren't legal while a render pass is active.
    if (have_text) {
        record_text_atlas_upload(g_renderer.command_buffer,
                                 atlas.pixels.data(),
                                 static_cast<std::uint32_t>(atlas.width),
                                 static_cast<std::uint32_t>(atlas.height));
    }
    // Image atlas dirty-region upload: no-op if no images were decoded
    // or the atlas view is already up to date.
    record_image_atlas_upload(g_renderer.command_buffer);

    VkClearValue clear{};
    std::memcpy(clear.color.float32, scratch.clear_color, sizeof scratch.clear_color);

    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = g_renderer.render_pass;
    rp.framebuffer = g_renderer.framebuffers[idx];
    rp.renderArea.offset = {0, 0};
    rp.renderArea.extent = g_renderer.swapchain_extent;
    rp.clearValueCount = 1;
    rp.pClearValues = &clear;
    vkCmdBeginRenderPass(g_renderer.command_buffer, &rp,
                         VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp{};
    vp.x = 0.0f; vp.y = 0.0f;
    vp.width = static_cast<float>(g_renderer.swapchain_extent.width);
    vp.height = static_cast<float>(g_renderer.swapchain_extent.height);
    vp.minDepth = 0.0f; vp.maxDepth = 1.0f;
    vkCmdSetViewport(g_renderer.command_buffer, 0, 1, &vp);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = g_renderer.swapchain_extent;
    vkCmdSetScissor(g_renderer.command_buffer, 0, 1, &scissor);

    if (!scratch.color_instances.empty()) {
        vkCmdBindPipeline(g_renderer.command_buffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          g_renderer.color_pipeline);
        vkCmdBindDescriptorSets(g_renderer.command_buffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                g_renderer.color_pipeline_layout,
                                0, 1, &g_renderer.color_descriptor_set,
                                0, nullptr);
        vkCmdDraw(g_renderer.command_buffer, 6,
                  static_cast<std::uint32_t>(scratch.color_instances.size()),
                  0, 0);
    }

    if (have_text) {
        vkCmdBindPipeline(g_renderer.command_buffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          g_renderer.text_pipeline);
        vkCmdBindDescriptorSets(g_renderer.command_buffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                g_renderer.text_pipeline_layout,
                                0, 1, &g_renderer.text_descriptor_set,
                                0, nullptr);
        vkCmdDraw(g_renderer.command_buffer, 6,
                  static_cast<std::uint32_t>(scratch.text_instances.size()),
                  0, 0);
    }

    if (have_images && g_renderer.image_atlas_initialised) {
        vkCmdBindPipeline(g_renderer.command_buffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          g_renderer.image_pipeline);
        vkCmdBindDescriptorSets(g_renderer.command_buffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                g_renderer.image_pipeline_layout,
                                0, 1, &g_renderer.image_descriptor_set,
                                0, nullptr);
        vkCmdDraw(g_renderer.command_buffer, 6,
                  static_cast<std::uint32_t>(scratch.image_instances.size()),
                  0, 0);
    }

    vkCmdEndRenderPass(g_renderer.command_buffer);

    // Stage 7: snapshot the presented frame into debug_capture_image
    // so capture_frame_rgba() has a fresh copy without re-rendering.
    record_debug_capture_copy(g_renderer.command_buffer, idx);

    vkEndCommandBuffer(g_renderer.command_buffer);

    VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &g_renderer.image_available;
    si.pWaitDstStageMask = &wait;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &g_renderer.command_buffer;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &g_renderer.render_finished;
    vkQueueSubmit(g_renderer.queue, 1, &si, g_renderer.in_flight);

    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &g_renderer.render_finished;
    pi.swapchainCount = 1;
    pi.pSwapchains = &g_renderer.swapchain;
    pi.pImageIndices = &idx;
    auto pr = vkQueuePresentKHR(g_renderer.queue, &pi);
    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) {
        destroy_swapchain_resources();
        create_swapchain();
    }
}

inline void destroy_color_resources() {
    if (g_renderer.color_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(g_renderer.device,
                                g_renderer.color_descriptor_pool, nullptr);
        g_renderer.color_descriptor_pool = VK_NULL_HANDLE;
        g_renderer.color_descriptor_set = VK_NULL_HANDLE;
    }
    destroy_host_buffer(g_renderer.instance_buffer,
                        g_renderer.instance_memory,
                        g_renderer.instance_mapped);
    g_renderer.instance_capacity = 0;
    destroy_host_buffer(g_renderer.uniform_buffer,
                        g_renderer.uniform_memory,
                        g_renderer.uniform_mapped);
    if (g_renderer.color_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(g_renderer.device, g_renderer.color_pipeline, nullptr);
        g_renderer.color_pipeline = VK_NULL_HANDLE;
    }
    if (g_renderer.color_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(g_renderer.device,
                                g_renderer.color_pipeline_layout, nullptr);
        g_renderer.color_pipeline_layout = VK_NULL_HANDLE;
    }
    if (g_renderer.color_descriptor_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(g_renderer.device,
                                     g_renderer.color_descriptor_set_layout,
                                     nullptr);
        g_renderer.color_descriptor_set_layout = VK_NULL_HANDLE;
    }
    if (g_renderer.render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(g_renderer.device, g_renderer.render_pass, nullptr);
        g_renderer.render_pass = VK_NULL_HANDLE;
    }
}

inline void renderer_shutdown() {
    if (g_renderer.device != VK_NULL_HANDLE) vkDeviceWaitIdle(g_renderer.device);
    destroy_swapchain_resources();
    destroy_debug_readback_buffer();
    destroy_image_pipeline_resources();
    reset_image_cache();
    destroy_text_pipeline_resources();
    destroy_color_resources();
    if (g_renderer.in_flight != VK_NULL_HANDLE) {
        vkDestroyFence(g_renderer.device, g_renderer.in_flight, nullptr);
        g_renderer.in_flight = VK_NULL_HANDLE;
    }
    if (g_renderer.render_finished != VK_NULL_HANDLE) {
        vkDestroySemaphore(g_renderer.device, g_renderer.render_finished, nullptr);
        g_renderer.render_finished = VK_NULL_HANDLE;
    }
    if (g_renderer.image_available != VK_NULL_HANDLE) {
        vkDestroySemaphore(g_renderer.device, g_renderer.image_available, nullptr);
        g_renderer.image_available = VK_NULL_HANDLE;
    }
    if (g_renderer.command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(g_renderer.device, g_renderer.command_pool, nullptr);
        g_renderer.command_pool = VK_NULL_HANDLE;
    }
    if (g_renderer.device != VK_NULL_HANDLE) {
        vkDestroyDevice(g_renderer.device, nullptr);
        g_renderer.device = VK_NULL_HANDLE;
    }
    if (g_renderer.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_renderer.instance, g_renderer.surface, nullptr);
        g_renderer.surface = VK_NULL_HANDLE;
    }
    if (g_renderer.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(g_renderer.instance, nullptr);
        g_renderer.instance = VK_NULL_HANDLE;
    }
    g_renderer.window = nullptr;
    g_renderer.initialized = false;
}

inline std::optional<unsigned int> renderer_hit_test(float x, float y,
                                                     float scroll_y) {
    if (g_renderer.last_frame_buf.empty()) return std::nullopt;
    auto commands = ::phenotype::parse_commands(
        g_renderer.last_frame_buf.data(),
        static_cast<unsigned int>(g_renderer.last_frame_buf.size()));
    // Walk in reverse — last drawn = topmost in macOS / Windows model.
    for (auto it = commands.rbegin(); it != commands.rend(); ++it) {
        if (auto const* hr =
                std::get_if<::phenotype::HitRegionCmd>(&*it)) {
            float const y_adj = y + scroll_y;
            if (x >= hr->x && x <= hr->x + hr->w
                && y_adj >= hr->y && y_adj <= hr->y + hr->h) {
                return hr->callback_id;
            }
        }
    }
    return std::nullopt;
}

// ---- Stage 6 input_api: Android-specific slots ----------------------
//
// Stage 6 takes over the four stub slots (handle_cursor_pos,
// handle_mouse_button, dismiss_transient, scroll_delta_y) and replaces
// attach/detach/sync with Android-flavoured versions. The shell does
// the actual work; these functions return false to signal "platform
// didn't consume, let the shell do its hit-test/focus/repaint dance".

inline void (*g_android_request_repaint)() = nullptr;

inline void android_input_attach(native_surface_handle /*surface*/,
                                 void (*request_repaint)()) {
    // Stage 6: we render every tick from the Gradle driver's main loop,
    // so request_repaint is a notification signal rather than a trigger.
    // Stored for a future dirty-flag optimisation (Stage 7).
    g_android_request_repaint = request_repaint;
}

inline void android_input_detach() {
    g_android_request_repaint = nullptr;
}

inline void android_input_sync() {
    // No Android-side IME composition in Stage 6. GameTextInput is Stage 7.
}

inline bool android_input_uses_shared_caret_blink() { return true; }

inline bool android_input_handle_cursor_pos(float /*x*/, float /*y*/) {
    return false; // let the shell update hover + focus
}

inline bool android_input_handle_mouse_button(float /*x*/, float /*y*/,
                                              int /*button*/,
                                              int /*action*/,
                                              int /*mods*/) {
    return false; // shell runs hit-test + callback dispatch
}

inline bool android_input_dismiss_transient() {
    return false; // no transient overlay to dismiss until Stage 7
}

inline float android_input_scroll_delta_y(double dy, float line_height,
                                          float /*viewport*/) {
    // Stage 6 doesn't surface scroll events yet, but keep the math
    // consistent with the desktop default so the hook is ready when
    // we wire AMOTION_EVENT_ACTION_SCROLL in Stage 7.
    return static_cast<float>(dy) * line_height * 3.0f;
}

inline void android_open_url(char const* url, unsigned int len) {
    __android_log_print(ANDROID_LOG_INFO, ANDROID_LOG_TAG,
                        "open_url ignored: %.*s", static_cast<int>(len), url);
}

// ---- Stage 7 debug plane --------------------------------------------

inline ::phenotype::diag::PlatformCapabilitiesSnapshot android_debug_capabilities() {
    return {
        /*platform=*/"android",
        /*read_only=*/true,
        /*snapshot_json=*/true,
        /*capture_frame_rgba=*/true,
        /*write_artifact_bundle=*/true,
        /*semantic_tree=*/true,
        /*input_debug=*/true,
        /*platform_runtime=*/true,
        /*frame_image=*/true,
        /*platform_diagnostics=*/true,
    };
}

inline ::json::Object android_renderer_runtime_json() {
    ::json::Object r;
    r.emplace("initialized", ::json::Value{g_renderer.initialized});
    r.emplace("swapchain_width",
        ::json::Value{static_cast<std::int64_t>(g_renderer.swapchain_extent.width)});
    r.emplace("swapchain_height",
        ::json::Value{static_cast<std::int64_t>(g_renderer.swapchain_extent.height)});
    r.emplace("last_render_width",
        ::json::Value{static_cast<std::int64_t>(g_renderer.last_render_width)});
    r.emplace("last_render_height",
        ::json::Value{static_cast<std::int64_t>(g_renderer.last_render_height)});
    r.emplace("last_frame_available",
        ::json::Value{g_renderer.last_frame_available});
    r.emplace("debug_capture_ready",
        ::json::Value{g_renderer.debug_capture_ready});
    r.emplace("text_pipeline_ready",
        ::json::Value{g_renderer.text_pipeline != VK_NULL_HANDLE});
    r.emplace("image_pipeline_ready",
        ::json::Value{g_renderer.image_pipeline != VK_NULL_HANDLE});
    r.emplace("color_pipeline_ready",
        ::json::Value{g_renderer.color_pipeline != VK_NULL_HANDLE});
    return r;
}

inline ::json::Value android_platform_runtime_details_json_with_reason(
        std::string_view reason) {
    ::json::Object root;
    root.emplace("backend", ::json::Value{std::string("vulkan")});
    root.emplace("renderer",
        ::json::Value{android_renderer_runtime_json()});
    root.emplace("image_cache_size",
        ::json::Value{static_cast<std::int64_t>(g_images.cache.size())});
    root.emplace("text_jni_initialised",
        ::json::Value{g_jni.initialised});
    if (!reason.empty())
        root.emplace("reason", ::json::Value{std::string(reason)});
    return ::json::Value{std::move(root)};
}

inline ::json::Value android_platform_runtime_details_json() {
    return android_platform_runtime_details_json_with_reason({});
}

inline std::string android_snapshot_json() {
    return ::phenotype::detail::serialize_diag_snapshot_with_debug(
        android_debug_capabilities(),
        android_platform_runtime_details_json());
}

// Copies the persistent debug_capture_image into the host-visible
// readback buffer on demand, then returns an RGBA8 DebugFrameCapture.
// No re-render — relies on the per-frame copy that renderer_flush runs
// right after each render pass.
inline std::optional<DebugFrameCapture> android_capture_frame_rgba() {
    if (!g_renderer.initialized
        || !g_renderer.last_frame_available
        || !g_renderer.debug_capture_ready
        || g_renderer.last_render_width == 0
        || g_renderer.last_render_height == 0
        || g_renderer.device == VK_NULL_HANDLE
        || g_renderer.queue == VK_NULL_HANDLE
        || g_renderer.command_pool == VK_NULL_HANDLE)
        return std::nullopt;

    auto width  = g_renderer.last_render_width;
    auto height = g_renderer.last_render_height;
    VkDeviceSize const bytes_per_row =
        static_cast<VkDeviceSize>(width) * 4;
    VkDeviceSize const total = bytes_per_row * static_cast<VkDeviceSize>(height);
    if (!ensure_debug_readback_capacity(total)) return std::nullopt;

    // Ensure any pending frame submission has completed so the copy
    // below sees the most recent debug_capture_image contents.
    if (g_renderer.in_flight != VK_NULL_HANDLE)
        vkWaitForFences(g_renderer.device, 1, &g_renderer.in_flight,
                        VK_TRUE, UINT64_MAX);

    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = g_renderer.command_pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (!vk_ok(vkAllocateCommandBuffers(g_renderer.device, &alloc, &cmd),
              "vkAllocateCommandBuffers(debug_capture)"))
        return std::nullopt;

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);

    VkImageMemoryBarrier to_src{};
    to_src.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    to_src.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    to_src.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_src.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_src.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_src.image = g_renderer.debug_capture_image;
    to_src.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    to_src.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_src.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &to_src);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = width;
    region.bufferImageHeight = height;
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };
    vkCmdCopyImageToBuffer(cmd,
        g_renderer.debug_capture_image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        g_renderer.debug_readback_buffer,
        1, &region);

    VkImageMemoryBarrier to_general{};
    to_general = to_src;
    to_general.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_general.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    to_general.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    to_general.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &to_general);

    vkEndCommandBuffer(cmd);

    VkFence once = VK_NULL_HANDLE;
    VkFenceCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (!vk_ok(vkCreateFence(g_renderer.device, &fi, nullptr, &once),
              "vkCreateFence(debug_capture)")) {
        vkFreeCommandBuffers(g_renderer.device, g_renderer.command_pool,
                             1, &cmd);
        return std::nullopt;
    }

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(g_renderer.queue, 1, &si, once);
    vkWaitForFences(g_renderer.device, 1, &once, VK_TRUE, UINT64_MAX);
    vkDestroyFence(g_renderer.device, once, nullptr);
    vkFreeCommandBuffers(g_renderer.device, g_renderer.command_pool,
                         1, &cmd);

    DebugFrameCapture cap{};
    cap.width = width;
    cap.height = height;
    cap.rgba.resize(static_cast<std::size_t>(total));
    std::memcpy(cap.rgba.data(), g_renderer.debug_readback_mapped,
                static_cast<std::size_t>(total));
    return cap;
}

inline DebugArtifactBundleResult android_write_artifact_bundle(
        char const* directory, char const* reason) {
    auto snapshot = android_snapshot_json();
    auto runtime = ::json::emit(
        android_platform_runtime_details_json_with_reason(
            reason ? std::string_view{reason} : std::string_view{}));
    auto frame = android_capture_frame_rgba();
    return ::phenotype::diag::detail::write_debug_artifact_bundle(
        directory ? std::string_view{directory} : std::string_view{},
        "android",
        snapshot,
        runtime,
        frame ? &*frame : nullptr);
}

inline void install_android_debug_providers() {
    ::phenotype::diag::detail::set_platform_capabilities_provider(
        android_debug_capabilities);
    ::phenotype::diag::detail::set_platform_runtime_details_provider(
        android_platform_runtime_details_json);
}

// ---- Stage 6 demo app ------------------------------------------------
//
// Bakes a minimal counter app (State + Msg + view + update) into the
// phenotype library so the example driver can start a live view/update
// loop via a C ABI hook. Stage 6 scope is interaction plumbing, not
// app-level configurability — Stage 7+ will expose a generic
// `phenotype_android_run<T, U>` mechanism once the template
// instantiation story settles.

namespace demo6 {

struct Increment {};
struct Decrement {};
using Msg = std::variant<Increment, Decrement>;

struct State { int count = 0; };

inline void update(State& s, Msg m) {
    std::visit([&](auto const& msg) {
        using T = std::decay_t<decltype(msg)>;
        if constexpr (std::same_as<T, Increment>) s.count += 1;
        else if constexpr (std::same_as<T, Decrement>) s.count -= 1;
    }, m);
}

inline void view(State const& s) {
    ::phenotype::layout::scaffold(
        [] {
            ::phenotype::widget::text("phenotype · stage 6");
            ::phenotype::widget::text("touch + keyboard demo");
        },
        [&] {
            ::phenotype::layout::card([&] {
                ::phenotype::widget::text("Core Widgets");
                ::phenotype::layout::spacer(8);
                ::phenotype::widget::text(
                    std::string("Count: ") + std::to_string(s.count));
                ::phenotype::layout::spacer(12);
                ::phenotype::layout::row(
                    [&] { ::phenotype::widget::button<Msg>("−", Decrement{}); },
                    [&] { ::phenotype::widget::button<Msg>("+", Increment{}); }
                );
                ::phenotype::layout::spacer(12);
                ::phenotype::widget::text(
                    "Tap the buttons, or Tab to focus and press Enter / Space.");
            });
        },
        [] {
            ::phenotype::widget::text(
                "Stage 6: GameActivity input → phenotype shell.");
        }
    );
}

} // namespace demo6

inline platform_api build_android_platform() {
    auto api = make_stub_platform(
        "android",
        "[phenotype-native] Android Vulkan backend "
        "(stage 7: color + text + image + input + debug)");
    api.renderer = {
        renderer_init,
        renderer_flush,
        renderer_shutdown,
        renderer_hit_test,
    };
    api.text = {
        text_init,
        text_shutdown,
        text_measure,
        text_build_atlas,
    };
    api.input = {
        android_input_attach,
        android_input_detach,
        android_input_sync,
        android_input_uses_shared_caret_blink,
        android_input_handle_cursor_pos,
        android_input_handle_mouse_button,
        android_input_dismiss_transient,
        android_input_scroll_delta_y,
    };
    api.debug = {
        android_debug_capabilities,
        android_snapshot_json,
        android_capture_frame_rgba,
        android_write_artifact_bundle,
    };
    api.open_url = android_open_url;
    return api;
}

} // namespace phenotype::native::detail

export namespace phenotype::native {

inline platform_api const& android_platform() {
    detail::install_android_debug_providers();
    static platform_api api = detail::build_android_platform();
    return api;
}

} // namespace phenotype::native

// C ABI for the examples/android GameActivity glue. Keeping this entry
// surface as plain C avoids forcing the Gradle-built caller to import
// the phenotype modules (which would require sharing exon's PCM output
// across the split build). The static_host + bound platform match what
// `phenotype::native::detail::run_host` would set up for a templated
// app; Stage 3+ will swap in the full run_host once we have something
// worth drawing beyond the clear color.

namespace phenotype::native::detail {

inline native_host g_android_host{};

inline void bind_platform_once() {
    if (!g_android_host.platform)
        g_android_host.platform = &android_platform();
}

// ---- Stage 6 input dispatch helpers ---------------------------------
//
// Android key codes are a disjoint enum from GLFW's (and therefore from
// phenotype's neutral Key enum, which mirrors GLFW values so desktop
// can cast-through). We only translate the keys that
// shell::dispatch_key actually switches on — everything else maps to
// Key::Other, which the shell already treats as "no semantic match".

inline Key translate_android_keycode(int akc) {
    // AKEYCODE_* constants are from <android/keycodes.h>; duplicated as
    // int literals here to keep this helper includable in places that
    // don't pull the header.
    switch (akc) {
    case AKEYCODE_TAB:             return Key::Tab;
    case AKEYCODE_DEL:             return Key::Backspace; // named DEL, acts as BS
    case AKEYCODE_ENTER:           return Key::Enter;
    case AKEYCODE_NUMPAD_ENTER:    return Key::KpEnter;
    case AKEYCODE_SPACE:           return Key::Space;
    case AKEYCODE_DPAD_LEFT:       return Key::Left;
    case AKEYCODE_DPAD_RIGHT:      return Key::Right;
    case AKEYCODE_DPAD_UP:         return Key::Up;
    case AKEYCODE_DPAD_DOWN:       return Key::Down;
    case AKEYCODE_PAGE_UP:         return Key::PageUp;
    case AKEYCODE_PAGE_DOWN:       return Key::PageDown;
    case AKEYCODE_MOVE_HOME:       return Key::Home;
    case AKEYCODE_MOVE_END:        return Key::End;
    case AKEYCODE_ESCAPE:          return Key::Escape;
    case AKEYCODE_A:               return Key::A;
    default:                       return Key::Other;
    }
}

// Translate Android's KeyEvent meta flags to phenotype's Modifier
// bitmask. META_SHIFT_ON | META_CTRL_ON | META_ALT_ON | META_META_ON
// (Super) are 1 / 0x1000 / 0x02 / 0x10000 respectively per
// <android/input.h>; shift to match phenotype's GLFW-mirrored values.
inline int translate_android_mods(int android_meta) {
    int mods = 0;
    if (android_meta & AMETA_SHIFT_ON) mods |= static_cast<int>(Modifier::Shift);
    if (android_meta & AMETA_CTRL_ON)  mods |= static_cast<int>(Modifier::Control);
    if (android_meta & AMETA_ALT_ON)   mods |= static_cast<int>(Modifier::Alt);
    if (android_meta & AMETA_META_ON)  mods |= static_cast<int>(Modifier::Super);
    return mods;
}

} // namespace phenotype::native::detail

extern "C" {

__attribute__((visibility("default")))
void phenotype_android_bind_jvm(void* jvm) {
    namespace d = phenotype::native::detail;
    d::g_jni.vm = static_cast<JavaVM*>(jvm);
}

__attribute__((visibility("default")))
void phenotype_android_bind_assets(void* asset_manager) {
    namespace d = phenotype::native::detail;
    d::g_asset_mgr = static_cast<AAssetManager*>(asset_manager);
}

__attribute__((visibility("default")))
void phenotype_android_attach_surface(void* native_window) {
    namespace d = phenotype::native::detail;
    d::bind_platform_once();
    d::g_android_host.window = native_window;
    if (d::g_android_host.platform->text.init)
        d::g_android_host.platform->text.init();
    if (d::g_android_host.platform->renderer.init)
        d::g_android_host.platform->renderer.init(d::g_android_host.window);
    if (d::g_android_host.platform->input.attach)
        d::g_android_host.platform->input.attach(d::g_android_host.window, nullptr);

    // Stage 6: keep the native_host's cached framebuffer size in sync
    // with the real surface so layout + hit-testing use pixel-accurate
    // dimensions.
    if (auto* w = static_cast<ANativeWindow*>(native_window)) {
        auto pw = ANativeWindow_getWidth(w);
        auto ph = ANativeWindow_getHeight(w);
        if (pw > 0) d::g_android_host.cached_width_px = pw;
        if (ph > 0) d::g_android_host.cached_height_px = ph;
    }
}

__attribute__((visibility("default")))
void phenotype_android_detach_surface(void) {
    namespace d = phenotype::native::detail;
    if (!d::g_android_host.platform) return;
    if (d::g_android_host.platform->input.detach)
        d::g_android_host.platform->input.detach();
    if (d::g_android_host.platform->renderer.shutdown)
        d::g_android_host.platform->renderer.shutdown();
    if (d::g_android_host.platform->text.shutdown)
        d::g_android_host.platform->text.shutdown();
    d::g_android_host.window = nullptr;
}

__attribute__((visibility("default")))
void phenotype_android_draw_frame(void) {
    namespace d = phenotype::native::detail;
    if (!d::g_android_host.platform || !d::g_android_host.window) return;
    // Stage 6: driver ticks call here every frame. Drive the standard
    // caret-blink / IME-sync / repaint sequence the GLFW shell uses
    // between glfwPollEvents calls. repaint_current is a no-op until
    // phenotype_android_start_app wires the view/update loop; before
    // that, the screen will simply not re-present (first frame after
    // start_app will paint).
    d::tick_caret_blink();
    d::sync_platform_input();
    d::repaint_current();
}

__attribute__((visibility("default")))
char const* phenotype_android_startup_message(void) {
    namespace d = phenotype::native::detail;
    d::bind_platform_once();
    return d::g_android_host.platform->startup_message;
}

// ---- Stage 6: app bootstrap + input dispatch ------------------------

__attribute__((visibility("default")))
void phenotype_android_start_app(void) {
    namespace d = phenotype::native::detail;
    d::bind_platform_once();
    // Instantiate the baked-in counter demo's run_host. The shell's
    // bind_host + phenotype::run<State, Msg> wire the view/update
    // closures into global state and trigger an initial repaint.
    d::run_host<d::demo6::State, d::demo6::Msg>(
        d::g_android_host, d::demo6::view, d::demo6::update);
}

__attribute__((visibility("default")))
void phenotype_android_dispatch_pointer(float x, float y, int action) {
    namespace d = phenotype::native::detail;
    using ::phenotype::native::MouseButton;
    using ::phenotype::native::KeyAction;
    switch (action) {
    case 0: // DOWN
        d::dispatch_mouse_button(x, y, MouseButton::Left,
                                 KeyAction::Press, 0);
        break;
    case 1: // MOVE
        d::dispatch_cursor_pos(x, y);
        break;
    case 2: // UP / CANCEL
        d::dispatch_mouse_button(x, y, MouseButton::Left,
                                 KeyAction::Release, 0);
        break;
    default:
        break;
    }
}

__attribute__((visibility("default")))
void phenotype_android_dispatch_key(int android_keycode, int action,
                                    int android_meta) {
    namespace d = phenotype::native::detail;
    using ::phenotype::native::KeyAction;
    KeyAction ka;
    switch (action) {
    case 0: ka = KeyAction::Release; break;
    case 1: ka = KeyAction::Press;   break;
    case 2: ka = KeyAction::Repeat;  break;
    default: return;
    }
    d::dispatch_key(d::translate_android_keycode(android_keycode),
                    ka,
                    d::translate_android_mods(android_meta));
}

__attribute__((visibility("default")))
void phenotype_android_dispatch_char(unsigned int codepoint) {
    namespace d = phenotype::native::detail;
    if (codepoint == 0) return;
    d::dispatch_char(codepoint);
}

} // extern "C"
#endif
