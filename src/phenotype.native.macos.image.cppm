module;
#if !defined(__wasi__) && !defined(__ANDROID__) && defined(__APPLE__)
#include <array>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#endif

export module phenotype.native.macos.image;

#if !defined(__wasi__) && !defined(__ANDROID__) && defined(__APPLE__)
import cppx.http;
import cppx.http.system;
import cppx.resource;

export namespace phenotype::native::detail {

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
    std::uint64_t generation = 0;
    std::uint64_t dirty_base_generation = 0;
    int dirty_min_x = atlas_size;
    int dirty_min_y = atlas_size;
    int dirty_max_x = 0;
    int dirty_max_y = 0;
};

inline ImageAtlasCache g_images;

inline bool is_http_url(std::string const& url) {
    return cppx::resource::is_remote(url);
}

inline std::filesystem::path resolve_image_path(std::string const& url) {
    return cppx::resource::resolve_path(
        std::filesystem::current_path(),
        std::string_view{url});
}

inline void mark_image_cache_dirty(ImageAtlasCache& cache,
                                   int x, int y, int w, int h) {
    if (!cache.dirty)
        cache.dirty_base_generation = cache.generation;
    cache.dirty = true;
    ++cache.generation;
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
    g_images.generation = 0;
    g_images.dirty_base_generation = 0;
    g_images.dirty_min_x = ImageAtlasCache::atlas_size;
    g_images.dirty_min_y = ImageAtlasCache::atlas_size;
    g_images.dirty_max_x = 0;
    g_images.dirty_max_y = 0;
    g_images.queue_only_for_tests = false;
    g_images.stop_worker = false;
    if (!preserve_request_repaint)
        g_images.request_repaint = nullptr;
}

} // namespace phenotype::native::detail
#endif
