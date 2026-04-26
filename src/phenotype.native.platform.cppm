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

struct TextEntry {
    float x, y, font_size;
    bool mono;
    float r, g, b, a;
    std::string text;
    float line_height = 0;
};

struct text_api {
    void (*init)() = nullptr;
    void (*shutdown)() = nullptr;
    float (*measure)(float font_size, bool mono,
                     char const* text, unsigned int len) = nullptr;
    TextAtlas (*build_atlas)(std::vector<TextEntry> const& entries,
                             float backing_scale) = nullptr;
};

// Opaque native surface handle. Today this carries a GLFWwindow* on
// macOS / Windows; the upcoming Android backend will carry an
// ANativeWindow*. Platform backends cast back to their own type at
// the single entry point where they need it.
using native_surface_handle = void*;

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

struct platform_api {
    char const* name = "stub";
    bool enabled = true;
    text_api text{};
    renderer_api renderer{};
    input_api input{};
    debug_api debug{};
    void (*open_url)(char const* url, unsigned int len) = nullptr;
    char const* startup_message = nullptr;
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
