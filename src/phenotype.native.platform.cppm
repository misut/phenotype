module;

#if !defined(__wasi__)
#include <filesystem>
#include <fstream>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#endif

export module phenotype.native.platform;

#if !defined(__wasi__)
import phenotype.diag;
import phenotype.types;

export namespace phenotype::native {

struct TextQuad {
    float x, y, w, h;
    float u, v, uw, vh;
};

struct TextAtlas {
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    std::vector<TextQuad> quads;
};

// `family`, `weight`, `italic` track FontSpec for the rasterisation
// pass; `mono` is retained as a quick monospace-default selector for
// the common widget::text path that doesn't carry a family. Backends
// merge family-based and mono-based defaults at acquire time.
struct TextEntry {
    float x, y, font_size;
    bool mono;
    float r, g, b, a;
    std::string text;
    float line_height = 0;
    std::string family;
    FontWeight weight = FontWeight::Regular;
    FontStyle  style  = FontStyle::Upright;
    // Per-run rotation (radians, CCW about pivot (x, y), already in
    // canvas-frame). Backends that support text rotation pass this
    // straight through to the vertex shader's rigid-body transform.
    // Default 0.0f reproduces the axis-aligned atlas path bit-for-bit.
    float rotation = 0.0f;
    // Horizontal glyph stretch — matches FontSpec::width_factor. The
    // raster pass folds this into the atlas via the backend's font-
    // matrix (CoreText) / Paint.setTextScaleX (Android) so glyph
    // advances and bitmap widths both scale. 1.0f is the unstretched
    // identity.
    float width_factor = 1.0f;
};

struct text_api {
    void (*init)() = nullptr;
    void (*shutdown)() = nullptr;
    // Pixel width measurement. `flags` is bit0=mono, bit1=bold,
    // bit2=italic — matches the wire format. `font_family` is a
    // UTF-8 bytes/length pair (length 0 → backend default family).
    float (*measure)(float font_size, unsigned int flags,
                     char const* font_family, unsigned int family_len,
                     char const* text, unsigned int len) = nullptr;
    // Vertical metrics for the resolved face at `font_size`. Backends
    // that cannot resolve metrics fill the four out-params with 0.0f
    // — the shell wrapper turns that into a zero `FontMetrics{}` for
    // the caller. `flags` and `font_family` are encoded the same way
    // as `measure`. `out_cap_height` is the baseline-to-cap-top
    // distance (CTFontGetCapHeight on macOS); zero on backends that
    // don't expose it yet. Implementations are expected to be
    // lightweight enough to call once per text run during layout (the
    // same cost shape as `measure`).
    void (*metrics)(float font_size, unsigned int flags,
                    char const* font_family, unsigned int family_len,
                    float* out_ascent, float* out_descent,
                    float* out_leading,
                    float* out_cap_height) = nullptr;
    TextAtlas (*build_atlas)(std::vector<TextEntry> const& entries,
                             float backing_scale) = nullptr;
    // Register a TTF / OTF / TTC file with the process's font manager
    // and bind it to the given alias. Subsequent `FontSpec::family`
    // lookups for `family_alias` resolve to the registered face. Both
    // `family_alias` and `path` are UTF-8 byte / length pairs (length
    // 0 forbidden for either). Returns true on success; false when
    // the file is missing, malformed, or the backend lacks runtime
    // registration support — caller should treat that as "fall back
    // to the platform default" (the existing `measure` /
    // `build_atlas` paths already do that for unknown families).
    // Idempotent: re-registering an alias replaces the prior binding.
    bool (*register_font_file)(char const* family_alias,
                               unsigned int alias_len,
                               char const* path,
                               unsigned int path_len) = nullptr;
};

enum class NativeSurfaceKind {
    Unknown,
    MacOSWindow,
    Win32Window,
    AndroidWindow,
};

struct NativeSurfaceDescriptor {
    NativeSurfaceKind kind = NativeSurfaceKind::Unknown;
    void* window = nullptr;
    void* view = nullptr;
    int logical_width = 0;
    int logical_height = 0;
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    float content_scale = 1.0f;
};

// Opaque native surface handle. Desktop shells pass a
// NativeSurfaceDescriptor*. Android still passes its platform window directly
// so the C ABI surface stays stable.
using native_surface_handle = void*;

enum class WindowChromeStyle {
    System,
    IntegratedTitlebar,
};

struct WindowOptions {
    WindowChromeStyle chrome = WindowChromeStyle::System;
};

struct window_api {
    void (*configure)(native_surface_handle surface,
                      WindowOptions const* options) = nullptr;
};

struct renderer_api {
    void (*init)(native_surface_handle surface) = nullptr;
    void (*flush)(unsigned char const* buf, unsigned int len) = nullptr;
    void (*shutdown)() = nullptr;
    std::optional<unsigned int> (*hit_test)(float x, float y,
                                            float scroll_x,
                                            float scroll_y) = nullptr;
};

struct input_api {
    void (*attach)(native_surface_handle surface,
                   void (*request_repaint)()) = nullptr;
    void (*detach)() = nullptr;
    void (*sync)() = nullptr;
    bool (*uses_shared_caret_blink)() = nullptr;
    bool (*handle_cursor_pos)(float x, float y) = nullptr;
    bool (*handle_mouse_button)(float x, float y,
                                int button, int action, int mods) = nullptr;
    bool (*dismiss_transient)() = nullptr;
    float (*scroll_delta_y)(double dy,
                            float line_height,
                            float viewport_height) = nullptr;
};

using DebugFrameCapture = ::phenotype::diag::detail::ArtifactFrameCapture;
using DebugArtifactBundleResult = ::phenotype::diag::detail::ArtifactBundleResult;

struct debug_api {
    ::phenotype::diag::PlatformCapabilitiesSnapshot (*capabilities)() = nullptr;
    std::string (*snapshot_json)() = nullptr;
    std::optional<DebugFrameCapture> (*capture_frame_rgba)() = nullptr;
    DebugArtifactBundleResult (*write_artifact_bundle)(
        char const* directory,
        char const* reason) = nullptr;
};

struct dialog_api {
    // Open a native file-open dialog. `filter_extensions` is a
    // semicolon-separated list of extensions without leading dots
    // (e.g. "dwg" or "dwg;dxf"), or null to accept any file. The
    // backend invokes `callback` on the same thread that pumps the
    // view/update loop, so it is safe to dispatch a Msg from inside it:
    //   * `path` is null when the user cancels.
    //   * Otherwise `path` is a NUL-terminated UTF-8 filesystem path.
    //     For backends where the user's pick is not a filesystem entry
    //     (Android SAF returns a content:// URI) the backend stages
    //     the bytes to a temporary file in the app's cache directory
    //     and returns that path instead, so callers can hand it
    //     straight to file-based libraries like LibreDWG.
    // Modal backends (macOS NSOpenPanel.runModal) deliver the callback
    // synchronously inside this call; asynchronous backends (Android
    // SAF) return immediately and post the result later.
    void (*open_file)(char const* filter_extensions,
                      void (*callback)(char const* path)) = nullptr;
};

struct platform_api {
    char const* name = "stub";
    bool enabled = true;
    text_api text{};
    renderer_api renderer{};
    input_api input{};
    debug_api debug{};
    void (*open_url)(char const* url, unsigned int len) = nullptr;
    char const* startup_message = nullptr;
    dialog_api dialog{};
    window_api window{};
};

inline constexpr unsigned int invalid_callback_id = 0xFFFFFFFFu;

} // namespace phenotype::native

export namespace phenotype::native::detail {

inline void append_u16_le(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
}

inline void append_u32_le(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
}

inline bool ensure_directory(std::filesystem::path const& directory,
                             std::string& error) {
    std::error_code ec;
    if (directory.empty()) {
        error = "Artifact directory is empty";
        return false;
    }
    if (std::filesystem::create_directories(directory, ec) || std::filesystem::exists(directory))
        return true;
    error = ec ? ec.message() : "Failed to create artifact directory";
    return false;
}

inline bool write_text_file(std::filesystem::path const& path,
                            std::string_view contents,
                            std::string& error) {
    auto parent = path.parent_path();
    if (!parent.empty() && !ensure_directory(parent, error))
        return false;

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "Failed to open file for writing: " + path.string();
        return false;
    }
    out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!out.good()) {
        error = "Failed to write file: " + path.string();
        return false;
    }
    return true;
}

inline bool write_bmp_rgba(std::filesystem::path const& path,
                           DebugFrameCapture const& frame,
                           std::string& error) {
    if (frame.width == 0 || frame.height == 0) {
        error = "Frame capture is empty";
        return false;
    }
    auto expected_size =
        static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height) * 4u;
    if (frame.rgba.size() != expected_size) {
        error = "Frame capture size does not match width/height";
        return false;
    }

    std::vector<std::uint8_t> bytes;
    bytes.reserve(14 + 40 + frame.rgba.size());

    auto file_size = static_cast<std::uint32_t>(14 + 40 + frame.rgba.size());
    auto pixel_offset = static_cast<std::uint32_t>(14 + 40);

    bytes.push_back(static_cast<std::uint8_t>('B'));
    bytes.push_back(static_cast<std::uint8_t>('M'));
    append_u32_le(bytes, file_size);
    append_u16_le(bytes, 0);
    append_u16_le(bytes, 0);
    append_u32_le(bytes, pixel_offset);

    append_u32_le(bytes, 40);
    append_u32_le(bytes, frame.width);
    append_u32_le(bytes, static_cast<std::uint32_t>(-static_cast<std::int32_t>(frame.height)));
    append_u16_le(bytes, 1);
    append_u16_le(bytes, 32);
    append_u32_le(bytes, 0);
    append_u32_le(bytes, static_cast<std::uint32_t>(frame.rgba.size()));
    append_u32_le(bytes, 2835);
    append_u32_le(bytes, 2835);
    append_u32_le(bytes, 0);
    append_u32_le(bytes, 0);

    for (std::size_t index = 0; index < frame.rgba.size(); index += 4) {
        bytes.push_back(frame.rgba[index + 2]);
        bytes.push_back(frame.rgba[index + 1]);
        bytes.push_back(frame.rgba[index + 0]);
        bytes.push_back(frame.rgba[index + 3]);
    }

    return write_text_file(
        path,
        std::string_view(
            reinterpret_cast<char const*>(bytes.data()),
            bytes.size()),
        error);
}

} // namespace phenotype::native::detail
#endif
