module;
#ifndef __wasi__
#include <optional>
#include <variant>
#include <vector>

struct GLFWwindow;
#endif

export module phenotype.native.stub;

#ifndef __wasi__
import phenotype.commands;
import phenotype.native.platform;

namespace phenotype::native::detail {

inline std::vector<HitRegionCmd>& stub_hit_regions() {
    static std::vector<HitRegionCmd>& regions = *new std::vector<HitRegionCmd>();
    return regions;
}

inline void stub_text_init() {}
inline void stub_text_shutdown() {}

inline float stub_measure(float font_size, bool, char const*, unsigned int len) {
    return static_cast<float>(len) * font_size * 0.6f;
}

inline TextAtlas stub_build_atlas(std::vector<TextEntry> const&, float) {
    return {};
}

inline void stub_renderer_init(GLFWwindow*) {}

inline void stub_renderer_flush(unsigned char const* buf, unsigned int len) {
    auto& regions = stub_hit_regions();
    regions.clear();
    if (len == 0) return;

    auto cmds = parse_commands(buf, len);
    for (auto const& cmd : cmds) {
        if (auto const* hr = std::get_if<HitRegionCmd>(&cmd))
            regions.push_back(*hr);
    }
}

inline void stub_renderer_shutdown() {
    stub_hit_regions().clear();
}

inline std::optional<unsigned int> stub_renderer_hit_test(
        float x, float y, float scroll_y) {
    float wy = y + scroll_y;
    auto& regions = stub_hit_regions();
    for (int i = static_cast<int>(regions.size()) - 1; i >= 0; --i) {
        auto const& hr = regions[static_cast<std::size_t>(i)];
        if (x >= hr.x && x <= hr.x + hr.w && wy >= hr.y && wy <= hr.y + hr.h)
            return hr.callback_id;
    }
    return std::nullopt;
}

inline void stub_open_url(char const*, unsigned int) {}

} // namespace phenotype::native::detail

export namespace phenotype::native {

inline platform_api make_stub_platform(char const* name,
                                       char const* startup_message) {
    platform_api api{};
    api.name = name;
    api.enabled = true;
    api.text = {
        detail::stub_text_init,
        detail::stub_text_shutdown,
        detail::stub_measure,
        detail::stub_build_atlas,
    };
    api.renderer = {
        detail::stub_renderer_init,
        detail::stub_renderer_flush,
        detail::stub_renderer_shutdown,
        detail::stub_renderer_hit_test,
    };
    api.open_url = detail::stub_open_url;
    api.startup_message = startup_message;
    return api;
}

inline platform_api const& desktop_stub_platform() {
    static platform_api api = make_stub_platform(
        "desktop-stub",
        "[phenotype-native] using stub desktop backend; native renderer/text are not implemented on this platform");
    return api;
}

} // namespace phenotype::native
#endif
