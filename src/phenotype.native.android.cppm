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
#include <jni.h>
#endif

export module phenotype.native.android;

#if defined(__ANDROID__)
import std;
import cppx.unicode;
import phenotype;
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
    float clear_color[4]{0, 0, 0, 0};
    bool has_clear = false;
};

// Minimal render_backend satisfying the phenotype::host::render_backend
// concept so build_stage3_demo can drive phenotype::emit_* directly.
// Fixed-size backing: the 5-command demo scene fits comfortably inside
// 1 KiB. detail::ensure only flushes (resets buf_len) when the buffer
// would overflow; as long as CAPACITY covers every command, we never
// hit that path.
struct DemoCmdBuffer {
    static constexpr unsigned int CAPACITY = 1024;
    std::array<unsigned char, CAPACITY> storage{};
    unsigned int used = 0;

    unsigned char* buf() { return storage.data(); }
    unsigned int& buf_len() { return used; }
    unsigned int buf_size() const { return CAPACITY; }
    void ensure(unsigned int /*needed*/) { /* fixed capacity */ }
    void flush() { /* no-op; decoder reads (buf, used) directly */ }
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
        __android_log_print(ANDROID_LOG_INFO, ANDROID_LOG_TAG,
                            "text_init: JNI refs cached (stage 4 debug)");
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
    __android_log_print(ANDROID_LOG_INFO, ANDROID_LOG_TAG,
                        "text_build_atlas: %dx%d, %zu entries (stage 4 debug)",
                        out.width, out.height, entries.size());
    return out;
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
    if (!create_image_views_and_framebuffers()) return false;
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
            }
            // DrawText / DrawImage / HitRegion are ignored in Stage 3.
        }, cmd);
    }
}

// Stage 3 demo composition: emits all five color commands through the
// public phenotype::emit_* API so the decoder above is exercised
// end-to-end. Stage 6 replaces this with a real View / Update loop
// bound to the example driver's State.
inline void build_stage3_demo(FrameScratch& out) {
    DemoCmdBuffer cmd;
    auto const& theme = ::phenotype::current_theme();
    ::phenotype::emit_clear(cmd, theme.background);
    ::phenotype::emit_fill_rect(cmd, 40.0f, 160.0f, 420.0f, 220.0f,
        ::phenotype::Color{220, 60, 80, 255});
    ::phenotype::emit_stroke_rect(cmd, 500.0f, 160.0f, 540.0f, 220.0f, 8.0f,
        ::phenotype::Color{60, 140, 220, 255});
    ::phenotype::emit_round_rect(cmd, 40.0f, 440.0f, 1000.0f, 300.0f, 56.0f,
        ::phenotype::Color{90, 170, 90, 255});
    ::phenotype::emit_draw_line(cmd, 40.0f, 820.0f, 1040.0f, 820.0f, 6.0f,
        ::phenotype::Color{40, 40, 40, 255});
    decode_android_color_commands(cmd.buf(), cmd.buf_len(), out);
}

inline void renderer_flush(unsigned char const* buf, unsigned int len) {
    if (!g_renderer.initialized) return;

    FrameScratch scratch;
    if (buf != nullptr && len > 0) {
        decode_android_color_commands(buf, len, scratch);
    } else {
        build_stage3_demo(scratch);
    }
    if (!scratch.has_clear) {
        auto const& theme = ::phenotype::current_theme();
        normalize_color(theme.background, scratch.clear_color);
    }

    vkWaitForFences(g_renderer.device, 1, &g_renderer.in_flight, VK_TRUE, UINT64_MAX);

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

    vkCmdEndRenderPass(g_renderer.command_buffer);
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

inline std::optional<unsigned int> renderer_hit_test(float, float, float) {
    return std::nullopt;
}

inline void android_open_url(char const* url, unsigned int len) {
    __android_log_print(ANDROID_LOG_INFO, ANDROID_LOG_TAG,
                        "open_url ignored: %.*s", static_cast<int>(len), url);
}

inline platform_api build_android_platform() {
    auto api = make_stub_platform(
        "android",
        "[phenotype-native] Android Vulkan backend (stage 4: color + text)");
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
    api.open_url = android_open_url;
    return api;
}

} // namespace phenotype::native::detail

export namespace phenotype::native {

inline platform_api const& android_platform() {
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

} // namespace phenotype::native::detail

extern "C" {

__attribute__((visibility("default")))
void phenotype_android_bind_jvm(void* jvm) {
    namespace d = phenotype::native::detail;
    d::g_jni.vm = static_cast<JavaVM*>(jvm);
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
    if (d::g_android_host.platform->renderer.flush)
        d::g_android_host.platform->renderer.flush(d::g_android_host.buffer, 0);
}

__attribute__((visibility("default")))
char const* phenotype_android_startup_message(void) {
    namespace d = phenotype::native::detail;
    d::bind_platform_once();
    return d::g_android_host.platform->startup_message;
}

} // extern "C"
#endif
