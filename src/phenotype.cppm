module;

#if defined(__wasi__)
#define PHENOTYPE_IMPORT(module, name) \
    __attribute__((import_module(module), import_name(name)))
#define PHENOTYPE_EXPORT(name) __attribute__((export_name(name)))
#else
#define PHENOTYPE_IMPORT(module, name)
#define PHENOTYPE_EXPORT(name)
#endif

export module phenotype;

import std;

#if defined(__wasi__)
extern "C" {
PHENOTYPE_IMPORT("phenotype", "flush")
void phenotype_flush(void);
PHENOTYPE_IMPORT("phenotype", "measure_text")
float phenotype_measure_text(float font_size,
                             unsigned int flags,
                             char const* font_family,
                             unsigned int family_len,
                             char const* text,
                             unsigned int len);
PHENOTYPE_IMPORT("phenotype", "get_canvas_width")
float phenotype_get_canvas_width(void);
PHENOTYPE_IMPORT("phenotype", "get_canvas_height")
float phenotype_get_canvas_height(void);
PHENOTYPE_IMPORT("phenotype", "open_url")
void phenotype_open_url(char const* url, unsigned int len);
}
#else
extern "C" {
void phenotype_flush(void) {}
float phenotype_measure_text(float font_size,
                             unsigned int,
                             char const*,
                             unsigned int,
                             char const*,
                             unsigned int len) {
    return static_cast<float>(len) * font_size * 0.56f;
}
float phenotype_get_canvas_width(void) { return 960.0f; }
float phenotype_get_canvas_height(void) { return 720.0f; }
void phenotype_open_url(char const*, unsigned int) {}
}
#endif

extern "C" {
inline constexpr unsigned int PHENOTYPE_CMD_BUF_SIZE = 2u * 1024u * 1024u;
alignas(4) unsigned char phenotype_cmd_buf[PHENOTYPE_CMD_BUF_SIZE] = {};
unsigned int phenotype_cmd_len = 0;
}

export namespace phenotype {

struct Color {
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
    unsigned char a = 255;
};

struct PaintRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    Color color{};
};

enum class TextColor {
    Default,
    Muted,
    Accent,
    HeroFg,
    HeroMuted,
};

enum class SpaceToken {
    Xs,
    Sm,
    Md,
    Lg,
    Xl,
};

enum class CrossAxisAlignment {
    Start,
    Center,
    End,
};

enum class MainAxisAlignment {
    Start,
    Center,
    End,
    SpaceBetween,
};

namespace detail {

inline constexpr unsigned int invalid_id = 0xFFFFFFFFu;

inline float space(SpaceToken token) noexcept {
    switch (token) {
        case SpaceToken::Xs: return 4.0f;
        case SpaceToken::Sm: return 8.0f;
        case SpaceToken::Md: return 14.0f;
        case SpaceToken::Lg: return 24.0f;
        case SpaceToken::Xl: return 36.0f;
    }
    return 14.0f;
}

inline std::uint32_t pack(Color color) noexcept {
    return (static_cast<std::uint32_t>(color.r) << 24u)
        | (static_cast<std::uint32_t>(color.g) << 16u)
        | (static_cast<std::uint32_t>(color.b) << 8u)
        | static_cast<std::uint32_t>(color.a);
}

inline Color rgb(unsigned char r, unsigned char g, unsigned char b) noexcept {
    return Color{r, g, b, 255};
}

struct Palette {
    Color background = rgb(248, 250, 252);
    Color surface = rgb(255, 255, 255);
    Color surface_alt = rgb(241, 245, 249);
    Color text = rgb(31, 41, 55);
    Color muted = rgb(100, 116, 139);
    Color accent = rgb(27, 118, 96);
    Color accent_strong = rgb(16, 93, 76);
    Color accent_soft = rgb(224, 245, 238);
    Color border = rgb(203, 213, 225);
    Color warning = rgb(175, 91, 26);
    Color focus = rgb(37, 99, 235);
};

inline Palette const& palette() {
    static Palette value{};
    return value;
}

inline Color text_color(TextColor color) {
    auto const& p = palette();
    switch (color) {
        case TextColor::Muted:
        case TextColor::HeroMuted:
            return p.muted;
        case TextColor::Accent:
            return p.accent;
        case TextColor::HeroFg:
        case TextColor::Default:
        default:
            return p.text;
    }
}

template <typename T>
void write(T value) {
    if (phenotype_cmd_len + sizeof(T) > PHENOTYPE_CMD_BUF_SIZE)
        return;
    std::memcpy(phenotype_cmd_buf + phenotype_cmd_len, &value, sizeof(T));
    phenotype_cmd_len += static_cast<unsigned int>(sizeof(T));
}

inline void align4() {
    while ((phenotype_cmd_len & 3u) != 0u) {
        write<unsigned char>(0);
    }
}

inline void write_string(std::string_view value) {
    if (!value.empty()) {
        if (phenotype_cmd_len + value.size() > PHENOTYPE_CMD_BUF_SIZE)
            return;
        std::memcpy(phenotype_cmd_buf + phenotype_cmd_len,
                    value.data(),
                    value.size());
        phenotype_cmd_len += static_cast<unsigned int>(value.size());
    }
    align4();
}

enum class Command : std::uint32_t {
    End = 0,
    Clear = 1,
    FillRect = 2,
    StrokeRect = 3,
    RoundRect = 4,
    DrawText = 5,
    DrawLine = 6,
    HitRegion = 7,
    DrawImage = 8,
    FillRects = 14,
};

inline void clear(Color color) {
    write<std::uint32_t>(static_cast<std::uint32_t>(Command::Clear));
    write<std::uint32_t>(pack(color));
}

inline void fill_rect(float x, float y, float w, float h, Color color) {
    write<std::uint32_t>(static_cast<std::uint32_t>(Command::FillRect));
    write<float>(x);
    write<float>(y);
    write<float>(w);
    write<float>(h);
    write<std::uint32_t>(pack(color));
}

inline void stroke_rect(float x,
                        float y,
                        float w,
                        float h,
                        float line_width,
                        Color color) {
    write<std::uint32_t>(static_cast<std::uint32_t>(Command::StrokeRect));
    write<float>(x);
    write<float>(y);
    write<float>(w);
    write<float>(h);
    write<float>(line_width);
    write<std::uint32_t>(pack(color));
}

inline void round_rect(float x,
                       float y,
                       float w,
                       float h,
                       float radius,
                       Color color) {
    write<std::uint32_t>(static_cast<std::uint32_t>(Command::RoundRect));
    write<float>(x);
    write<float>(y);
    write<float>(w);
    write<float>(h);
    write<float>(radius);
    write<std::uint32_t>(pack(color));
}

inline void draw_line(float x1,
                      float y1,
                      float x2,
                      float y2,
                      float thickness,
                      Color color) {
    write<std::uint32_t>(static_cast<std::uint32_t>(Command::DrawLine));
    write<float>(x1);
    write<float>(y1);
    write<float>(x2);
    write<float>(y2);
    write<float>(thickness);
    write<std::uint32_t>(pack(color));
}

inline unsigned int font_flags(bool mono = false,
                               bool bold = false,
                               bool italic = false) noexcept {
    return (mono ? 1u : 0u) | (bold ? 2u : 0u) | (italic ? 4u : 0u);
}

inline float measure_text(std::string_view text,
                          float font_size,
                          bool mono = false,
                          bool bold = false,
                          bool italic = false,
                          std::string_view family = {}) {
    return phenotype_measure_text(font_size,
                                  font_flags(mono, bold, italic),
                                  family.data(),
                                  static_cast<unsigned int>(family.size()),
                                  text.data(),
                                  static_cast<unsigned int>(text.size()));
}

inline void draw_text(float x,
                      float y,
                      std::string_view text,
                      float font_size,
                      Color color,
                      bool mono = false,
                      bool bold = false,
                      std::string_view family = {}) {
    write<std::uint32_t>(static_cast<std::uint32_t>(Command::DrawText));
    write<float>(x);
    write<float>(y);
    write<float>(font_size);
    write<float>(0.0f);
    write<float>(1.0f);
    write<std::uint32_t>(font_flags(mono, bold, false));
    write<std::uint32_t>(pack(color));
    write<std::uint32_t>(static_cast<std::uint32_t>(family.size()));
    write_string(family);
    write<std::uint32_t>(static_cast<std::uint32_t>(text.size()));
    write_string(text);
}

inline void draw_image(float x,
                       float y,
                       float w,
                       float h,
                       std::string_view url) {
    write<std::uint32_t>(static_cast<std::uint32_t>(Command::DrawImage));
    write<float>(x);
    write<float>(y);
    write<float>(w);
    write<float>(h);
    write<std::uint32_t>(static_cast<std::uint32_t>(url.size()));
    write_string(url);
}

inline void hit_region(float x,
                       float y,
                       float w,
                       float h,
                       unsigned int callback_id,
                       unsigned int cursor_type = 1u) {
    if (callback_id == invalid_id)
        return;
    write<std::uint32_t>(static_cast<std::uint32_t>(Command::HitRegion));
    write<float>(x);
    write<float>(y);
    write<float>(w);
    write<float>(h);
    write<std::uint32_t>(callback_id);
    write<std::uint32_t>(cursor_type);
}

inline void fill_rects(PaintRect const* rects, unsigned int count) {
    write<std::uint32_t>(static_cast<std::uint32_t>(Command::FillRects));
    write<std::uint32_t>(count);
    for (unsigned int i = 0; i < count; ++i) {
        write<float>(rects[i].x);
        write<float>(rects[i].y);
        write<float>(rects[i].w);
        write<float>(rects[i].h);
        write<std::uint32_t>(pack(rects[i].color));
    }
}

struct StateStore {
    std::vector<std::pair<std::string, std::any>> values;
};

struct InputHandler {
    unsigned int id = invalid_id;
    std::function<std::string()> get;
    std::function<void(std::string)> set;
};

struct Runtime {
    std::vector<std::function<void()>> callbacks;
    std::vector<InputHandler> inputs;
    std::function<void()> runner;
    float viewport_width = 960.0f;
    float viewport_height = 720.0f;
    float total_height = 0.0f;
    float scroll_y = 0.0f;
    unsigned int focused_id = invalid_id;
    unsigned int generation = 0;
};

inline Runtime& runtime() {
    // Browser hosts keep calling exported event handlers after WASI
    // `_start` returns, so the UI runtime must outlive C++ shutdown.
    alignas(Runtime) static std::byte storage[sizeof(Runtime)];
    static Runtime* value = std::construct_at(reinterpret_cast<Runtime*>(storage));
    return *value;
}

inline unsigned int register_callback(std::function<void()> callback) {
    auto& rt = runtime();
    auto id = static_cast<unsigned int>(rt.callbacks.size());
    rt.callbacks.push_back(std::move(callback));
    return id;
}

inline unsigned int register_input(std::function<std::string()> get,
                                   std::function<void(std::string)> set) {
    auto id = register_callback([] {});
    runtime().inputs.push_back(InputHandler{
        .id = id,
        .get = std::move(get),
        .set = std::move(set),
    });
    return id;
}

inline InputHandler* focused_input() {
    auto& inputs = runtime().inputs;
    auto focused = runtime().focused_id;
    auto it = std::ranges::find_if(inputs, [focused](InputHandler& input) {
        return input.id == focused;
    });
    return it == inputs.end() ? nullptr : std::addressof(*it);
}

inline void trigger_rebuild() {
    auto runner = runtime().runner;
    if (runner)
        runner();
}

enum class Font {
    Body,
    Title,
    Caption,
    Code,
    HeroTitle,
    HeroSubtitle,
};

inline float font_size(Font font) noexcept {
    switch (font) {
        case Font::HeroTitle: return 58.0f;
        case Font::HeroSubtitle: return 25.0f;
        case Font::Title: return 26.0f;
        case Font::Caption: return 13.0f;
        case Font::Code: return 14.0f;
        case Font::Body:
        default: return 16.0f;
    }
}

inline bool font_mono(Font font) noexcept {
    return font == Font::Code;
}

inline bool font_bold(Font font) noexcept {
    return font == Font::HeroTitle || font == Font::Title;
}

struct Size {
    float width = 0.0f;
    float height = 0.0f;
};

struct Element;
using Children = std::vector<Element>;

enum class ElementKind {
    VStack,
    HStack,
    Box,
    Weighted,
    SizedBox,
    Card,
    ScrollView,
    Text,
    Code,
    Link,
    Image,
    Button,
    TextField,
    Checkbox,
    Radio,
    Switch,
    Tabs,
    Progress,
    Divider,
    Spacer,
    Canvas,
};

struct StackOptions {
    SpaceToken spacing = SpaceToken::Md;
    CrossAxisAlignment cross = CrossAxisAlignment::Start;
    MainAxisAlignment main = MainAxisAlignment::Start;
};

class Painter;

struct Element {
    ElementKind kind = ElementKind::Box;
    Children children;
    StackOptions stack{};
    std::string text;
    std::string href;
    std::vector<std::string> items;
    Font font = Font::Body;
    TextColor text_color = TextColor::Default;
    float width = 0.0f;
    float height = -1.0f;
    float weight = 0.0f;
    float progress = 0.0f;
    std::size_t selected = 0;
    bool enabled = true;
    bool checked = false;
    bool primary = false;
    unsigned int callback_id = invalid_id;
    std::vector<unsigned int> callback_ids;
    std::function<void(Painter&)> paint;
};

inline std::vector<Element>*& current_children() {
    static thread_local std::vector<Element>* value = nullptr;
    return value;
}

inline void append(Element element) {
    if (auto* children = current_children())
        children->push_back(std::move(element));
}

template <typename F>
inline Element make_container(ElementKind kind, StackOptions options, F&& body) {
    Element element;
    element.kind = kind;
    element.stack = options;
    auto* previous = current_children();
    current_children() = &element.children;
    std::forward<F>(body)();
    current_children() = previous;
    return element;
}

class Painter {
    float origin_x_ = 0.0f;
    float origin_y_ = 0.0f;

public:
    Painter(float x, float y) : origin_x_(x), origin_y_(y) {}

    void fill_rects(PaintRect const* rects, unsigned int count) {
        auto translated = std::vector<PaintRect>{};
        translated.reserve(count);
        for (unsigned int i = 0; i < count; ++i) {
            auto rect = rects[i];
            rect.x += origin_x_;
            rect.y += origin_y_;
            translated.push_back(rect);
        }
        detail::fill_rects(translated.data(), count);
    }

    void line(float x1,
              float y1,
              float x2,
              float y2,
              float thickness,
              Color color) {
        detail::draw_line(
            origin_x_ + x1,
            origin_y_ + y1,
            origin_x_ + x2,
            origin_y_ + y2,
            thickness,
            color);
    }

    void text(float x,
              float y,
              char const* text,
              unsigned int len,
              float font_size,
              Color color) {
        detail::draw_text(
            origin_x_ + x,
            origin_y_ + y,
            std::string_view{text, len},
            font_size,
            color);
    }
};

inline std::vector<std::string> wrap_text(std::string_view text,
                                          float width,
                                          float size,
                                          bool mono,
                                          bool bold) {
    auto result = std::vector<std::string>{};
    auto append_paragraph = [&](std::string_view paragraph) {
        if (paragraph.empty()) {
            result.emplace_back();
            return;
        }
        auto current = std::string{};
        std::size_t pos = 0;
        while (pos < paragraph.size()) {
            while (pos < paragraph.size() && paragraph[pos] == ' ')
                ++pos;
            auto next = paragraph.find(' ', pos);
            auto word = paragraph.substr(
                pos,
                next == std::string_view::npos ? paragraph.size() - pos : next - pos);
            if (word.empty())
                break;
            auto candidate = current.empty()
                ? std::string(word)
                : current + " " + std::string(word);
            if (!current.empty()
                && detail::measure_text(candidate, size, mono, bold) > width) {
                result.push_back(current);
                current = std::string(word);
            } else {
                current = std::move(candidate);
            }
            if (next == std::string_view::npos)
                break;
            pos = next + 1;
        }
        if (!current.empty())
            result.push_back(current);
    };

    std::size_t start = 0;
    while (start <= text.size()) {
        auto end = text.find('\n', start);
        append_paragraph(text.substr(
            start,
            end == std::string_view::npos ? text.size() - start : end - start));
        if (end == std::string_view::npos)
            break;
        start = end + 1;
    }
    if (result.empty())
        result.emplace_back();
    return result;
}

inline Size measure(Element const& element, float width);

inline Size measure_children_vertical(Children const& children,
                                      float width,
                                      float gap) {
    auto size = Size{};
    for (auto const& child : children) {
        auto child_size = measure(child, width);
        size.width = std::max(size.width, child_size.width);
        size.height += child_size.height;
    }
    if (!children.empty())
        size.height += gap * static_cast<float>(children.size() - 1);
    return size;
}

inline Size measure(Element const& element, float width) {
    auto const max_width = std::max(1.0f, width);
    switch (element.kind) {
        case ElementKind::Text:
        case ElementKind::Link: {
            auto size = font_size(element.font);
            auto lines = wrap_text(
                element.text,
                max_width,
                size,
                font_mono(element.font),
                font_bold(element.font));
            auto line_height = size * 1.42f;
            auto measured_width = 0.0f;
            for (auto const& line : lines) {
                measured_width = std::max(
                    measured_width,
                    detail::measure_text(
                        line,
                        size,
                        font_mono(element.font),
                        font_bold(element.font)));
            }
            return Size{std::min(max_width, measured_width),
                        line_height * static_cast<float>(lines.size())};
        }
        case ElementKind::Code: {
            auto lines = wrap_text(element.text, max_width - 28.0f, 14.0f, true, false);
            return Size{max_width,
                        26.0f + 19.0f * static_cast<float>(lines.size())};
        }
        case ElementKind::Button:
            return Size{element.width > 0.0f ? element.width : 150.0f,
                        element.height > 0.0f ? element.height : 40.0f};
        case ElementKind::TextField:
            return Size{element.width > 0.0f ? element.width : std::min(360.0f, max_width),
                        44.0f};
        case ElementKind::Checkbox:
        case ElementKind::Radio:
        case ElementKind::Switch:
            return Size{std::min(max_width, 360.0f), 32.0f};
        case ElementKind::Tabs:
            return Size{std::min(max_width, 300.0f), 42.0f};
        case ElementKind::Progress:
            return Size{element.width > 0.0f ? element.width : 220.0f, 16.0f};
        case ElementKind::Divider:
            return Size{max_width, 1.0f};
        case ElementKind::Spacer:
            return Size{max_width, std::max(0.0f, element.height)};
        case ElementKind::Canvas:
        case ElementKind::Image:
            return Size{element.width, std::max(0.0f, element.height)};
        case ElementKind::Card: {
            auto inner = measure_children_vertical(element.children, max_width - 40.0f, 0.0f);
            return Size{max_width, inner.height + 40.0f};
        }
        case ElementKind::SizedBox: {
            auto inner_width = element.width > 0.0f ? element.width : max_width;
            auto inner = measure_children_vertical(element.children, inner_width, 0.0f);
            return Size{inner_width, element.height > 0.0f ? element.height : inner.height};
        }
        case ElementKind::ScrollView: {
            auto inner = measure_children_vertical(element.children, max_width, 0.0f);
            return Size{max_width, element.height > 0.0f ? element.height : inner.height};
        }
        case ElementKind::HStack: {
            auto gap = space(element.stack.spacing);
            auto total_width = 0.0f;
            auto total_height = 0.0f;
            auto fixed_children = std::size_t{};
            for (auto const& child : element.children) {
                if (child.kind == ElementKind::Weighted)
                    continue;
                auto child_size = measure(child, max_width);
                total_width += child_size.width;
                total_height = std::max(total_height, child_size.height);
                ++fixed_children;
            }
            auto weighted_count = std::ranges::count_if(
                element.children,
                [](Element const& child) {
                    return child.kind == ElementKind::Weighted;
                });
            auto gaps = element.children.empty()
                ? 0.0f
                : gap * static_cast<float>(element.children.size() - 1);
            auto remaining = std::max(80.0f, max_width - total_width - gaps);
            auto weighted_width = weighted_count > 0
                ? remaining / static_cast<float>(weighted_count)
                : 0.0f;
            for (auto const& child : element.children) {
                if (child.kind != ElementKind::Weighted)
                    continue;
                auto child_size = measure(child, weighted_width);
                total_width += weighted_width;
                total_height = std::max(total_height, child_size.height);
            }
            (void)fixed_children;
            return Size{std::min(max_width, total_width + gaps), total_height};
        }
        case ElementKind::Weighted:
            if (element.children.empty())
                return Size{max_width, 0.0f};
            return measure(element.children.front(), max_width);
        case ElementKind::Box:
        case ElementKind::VStack:
        default:
            return measure_children_vertical(
                element.children,
                max_width,
                element.kind == ElementKind::VStack ? space(element.stack.spacing) : 0.0f);
    }
}

inline void paint(Element const& element, float x, float y, float width, float scroll_y);

inline void paint_text_block(Element const& element,
                             float x,
                             float y,
                             float width,
                             float scroll_y) {
    auto size = font_size(element.font);
    auto lines = wrap_text(
        element.text,
        width,
        size,
        font_mono(element.font),
        font_bold(element.font));
    auto line_height = size * 1.42f;
    auto color = text_color(element.text_color);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        draw_text(x,
                  y - scroll_y + line_height * static_cast<float>(i),
                  lines[i],
                  size,
                  color,
                  font_mono(element.font),
                  font_bold(element.font));
    }
    if (element.kind == ElementKind::Link)
        hit_region(x, y, width, line_height * static_cast<float>(lines.size()),
                   element.callback_id, 1u);
}

inline void paint_children_vertical(Children const& children,
                                    float x,
                                    float y,
                                    float width,
                                    float gap,
                                    float scroll_y) {
    auto cursor = y;
    for (auto const& child : children) {
        auto child_size = measure(child, width);
        paint(child, x, cursor, width, scroll_y);
        cursor += child_size.height + gap;
    }
}

inline void paint(Element const& element, float x, float y, float width, float scroll_y) {
    auto const& p = palette();
    switch (element.kind) {
        case ElementKind::Text:
        case ElementKind::Link:
            paint_text_block(element, x, y, width, scroll_y);
            break;
        case ElementKind::Code: {
            auto h = measure(element, width).height;
            round_rect(x, y - scroll_y, width, h, 8.0f, p.surface_alt);
            stroke_rect(x, y - scroll_y, width, h, 1.0f, p.border);
            auto lines = wrap_text(element.text, width - 28.0f, 14.0f, true, false);
            for (std::size_t i = 0; i < lines.size(); ++i) {
                draw_text(x + 14.0f,
                          y + 13.0f + 19.0f * static_cast<float>(i) - scroll_y,
                          lines[i],
                          14.0f,
                          p.text,
                          true);
            }
            break;
        }
        case ElementKind::Button: {
            auto size = measure(element, width);
            auto bg = element.primary ? p.accent : p.surface;
            auto fg = element.primary ? Color{255, 255, 255, 255} : p.text;
            if (!element.enabled) {
                bg = p.surface_alt;
                fg = p.muted;
            }
            round_rect(x, y - scroll_y, size.width, size.height, 7.0f, bg);
            stroke_rect(x, y - scroll_y, size.width, size.height, 1.0f,
                        element.primary ? p.accent : p.border);
            auto tw = measure_text(element.text, 15.0f, false, true);
            draw_text(x + (size.width - tw) * 0.5f,
                      y + 11.0f - scroll_y,
                      element.text,
                      15.0f,
                      fg,
                      false,
                      true);
            if (element.enabled)
                hit_region(x, y, size.width, size.height, element.callback_id, 1u);
            break;
        }
        case ElementKind::TextField: {
            auto size = measure(element, width);
            round_rect(x, y - scroll_y, size.width, size.height, 7.0f, p.surface);
            stroke_rect(x, y - scroll_y, size.width, size.height, 1.0f, p.border);
            auto value = element.href.empty() ? element.text : element.href;
            auto color = element.href.empty() ? p.muted : p.text;
            if (runtime().focused_id != element.callback_id) {
                draw_text(x + 12.0f, y + 11.0f - scroll_y, value, 16.0f, color);
            }
            hit_region(x, y, size.width, size.height, element.callback_id, 1u);
            break;
        }
        case ElementKind::Checkbox:
        case ElementKind::Radio:
        case ElementKind::Switch: {
            auto mark_size = element.kind == ElementKind::Switch ? 38.0f : 20.0f;
            auto mark_y = y + 4.0f;
            if (element.kind == ElementKind::Switch) {
                round_rect(x, mark_y - scroll_y, mark_size, 22.0f, 11.0f,
                           element.checked ? p.accent : p.border);
                round_rect(x + (element.checked ? 18.0f : 2.0f),
                           mark_y + 2.0f - scroll_y,
                           18.0f,
                           18.0f,
                           9.0f,
                           p.surface);
            } else {
                round_rect(x, mark_y - scroll_y, 20.0f, 20.0f,
                           element.kind == ElementKind::Radio ? 10.0f : 5.0f,
                           element.checked ? p.accent : p.surface);
                stroke_rect(x, mark_y - scroll_y, 20.0f, 20.0f, 1.2f,
                            element.checked ? p.accent : p.border);
            }
            draw_text(x + mark_size + 10.0f,
                      y + 5.0f - scroll_y,
                      element.text,
                      15.0f,
                      p.text);
            hit_region(x, y, width, 32.0f, element.callback_id, 1u);
            break;
        }
        case ElementKind::Tabs: {
            auto size = measure(element, width);
            auto count = std::max<std::size_t>(1, element.items.size());
            auto item_width = size.width / static_cast<float>(count);
            round_rect(x, y - scroll_y, size.width, size.height, 8.0f, p.surface_alt);
            for (std::size_t i = 0; i < element.items.size(); ++i) {
                auto item_x = x + item_width * static_cast<float>(i);
                if (i == element.selected) {
                    round_rect(item_x + 3.0f, y + 3.0f - scroll_y,
                               item_width - 6.0f, size.height - 6.0f,
                               6.0f, p.surface);
                }
                auto label = std::string_view{element.items[i]};
                auto tw = measure_text(label, 14.0f, false, i == element.selected);
                draw_text(item_x + (item_width - tw) * 0.5f,
                          y + 12.0f - scroll_y,
                          label,
                          14.0f,
                          i == element.selected ? p.accent : p.muted,
                          false,
                          i == element.selected);
                if (i < element.callback_ids.size())
                    hit_region(item_x, y, item_width, size.height,
                               element.callback_ids[i], 1u);
            }
            break;
        }
        case ElementKind::Progress: {
            auto size = measure(element, width);
            round_rect(x, y + 4.0f - scroll_y, size.width, 8.0f, 4.0f, p.surface_alt);
            round_rect(x, y + 4.0f - scroll_y,
                       std::max(0.0f, std::min(1.0f, element.progress)) * size.width,
                       8.0f, 4.0f, p.accent);
            break;
        }
        case ElementKind::Divider:
            fill_rect(x, y - scroll_y, width, 1.0f, p.border);
            break;
        case ElementKind::Canvas:
            if (element.paint) {
                Painter painter{x, y - scroll_y};
                element.paint(painter);
            }
            break;
        case ElementKind::Image:
            draw_image(x, y - scroll_y, element.width, element.height, element.text);
            break;
        case ElementKind::Card: {
            auto h = measure(element, width).height;
            round_rect(x, y - scroll_y, width, h, 8.0f, p.surface);
            stroke_rect(x, y - scroll_y, width, h, 1.0f, p.border);
            paint_children_vertical(element.children, x + 20.0f, y + 20.0f,
                                    width - 40.0f, 0.0f, scroll_y);
            break;
        }
        case ElementKind::SizedBox: {
            auto inner_width = element.width > 0.0f ? element.width : width;
            paint_children_vertical(element.children, x, y, inner_width, 0.0f, scroll_y);
            break;
        }
        case ElementKind::ScrollView:
            paint_children_vertical(element.children, x, y, width, 0.0f, scroll_y);
            break;
        case ElementKind::HStack: {
            auto gap = space(element.stack.spacing);
            auto row_width = measure(element, width).width;
            auto start_x = x;
            if (element.stack.main == MainAxisAlignment::Center)
                start_x += std::max(0.0f, (width - row_width) * 0.5f);
            else if (element.stack.main == MainAxisAlignment::End)
                start_x += std::max(0.0f, width - row_width);
            auto weighted_count = std::ranges::count_if(
                element.children,
                [](Element const& child) {
                    return child.kind == ElementKind::Weighted;
                });
            auto fixed_width = 0.0f;
            for (auto const& child : element.children) {
                if (child.kind != ElementKind::Weighted)
                    fixed_width += measure(child, width).width;
            }
            auto gaps = element.children.empty()
                ? 0.0f
                : gap * static_cast<float>(element.children.size() - 1);
            auto weighted_width = weighted_count > 0
                ? std::max(60.0f, (width - fixed_width - gaps)
                    / static_cast<float>(weighted_count))
                : 0.0f;
            auto cursor = start_x;
            for (auto const& child : element.children) {
                auto child_width = child.kind == ElementKind::Weighted
                    ? weighted_width
                    : measure(child, width).width;
                paint(child, cursor, y, child_width, scroll_y);
                cursor += child_width + gap;
            }
            break;
        }
        case ElementKind::Weighted:
            if (!element.children.empty())
                paint(element.children.front(), x, y, width, scroll_y);
            break;
        case ElementKind::Box:
        case ElementKind::VStack:
        default:
            paint_children_vertical(
                element.children,
                x,
                y,
                width,
                element.kind == ElementKind::VStack ? space(element.stack.spacing) : 0.0f,
                scroll_y);
            break;
    }
}

inline void render_elements(std::vector<Element> const& roots, float scroll_y) {
    auto& rt = runtime();
    phenotype_cmd_len = 0;
    clear(palette().background);
    auto width = std::max(320.0f, rt.viewport_width);
    auto content_height = measure_children_vertical(roots, width, 0.0f).height;
    rt.total_height = std::max(rt.viewport_height, content_height + 32.0f);
    paint_children_vertical(roots, 0.0f, 0.0f, width, 0.0f, scroll_y);
    write<std::uint32_t>(static_cast<std::uint32_t>(Command::End));
    phenotype_flush();
}

} // namespace detail

using Painter = detail::Painter;

namespace ui {

enum class Font {
    body,
    title,
    caption,
    code,
    hero_title,
    hero_subtitle,
};

inline detail::Font to_detail(Font font) noexcept {
    switch (font) {
        case Font::title: return detail::Font::Title;
        case Font::caption: return detail::Font::Caption;
        case Font::code: return detail::Font::Code;
        case Font::hero_title: return detail::Font::HeroTitle;
        case Font::hero_subtitle: return detail::Font::HeroSubtitle;
        case Font::body:
        default: return detail::Font::Body;
    }
}

template <typename T>
class Binding;

template <typename T>
class State {
    std::shared_ptr<T> value_;
    template <typename>
    friend class Binding;

public:
    State() = default;
    explicit State(std::shared_ptr<T> value) : value_(std::move(value)) {}

    T const& get() const noexcept { return *value_; }
    T& get() noexcept { return *value_; }

    void set(T value) const {
        *value_ = std::move(value);
        detail::trigger_rebuild();
    }

    template <typename F>
        requires std::invocable<F&, T&>
    void mutate(F&& fn) const {
        std::forward<F>(fn)(*value_);
        detail::trigger_rebuild();
    }

    Binding<T> binding() const noexcept;
};

template <typename T>
class Binding {
    std::shared_ptr<T> value_;

public:
    Binding() = default;
    explicit Binding(std::shared_ptr<T> value) : value_(std::move(value)) {}
    explicit Binding(State<T> const& state) : value_(state.value_) {}

    bool valid() const noexcept { return static_cast<bool>(value_); }
    T const& get() const noexcept { return *value_; }

    void set(T value) const {
        *value_ = std::move(value);
        detail::trigger_rebuild();
    }
};

template <typename T>
Binding<T> State<T>::binding() const noexcept {
    return Binding<T>{value_};
}

class Context {
    std::shared_ptr<detail::StateStore> store_;

public:
    explicit Context(std::shared_ptr<detail::StateStore> store)
        : store_(std::move(store)) {}

    template <typename T>
    State<T> state(std::string_view key,
                   T initial = T{},
                   std::source_location loc = std::source_location::current()) {
        auto storage_key = std::string{loc.file_name()}
            + ":"
            + std::to_string(loc.line())
            + ":"
            + std::string{key};
        auto it = std::ranges::find_if(store_->values, [&](auto const& entry) {
            return entry.first == storage_key;
        });
        if (it == store_->values.end()) {
            auto value = std::make_shared<T>(std::move(initial));
            store_->values.emplace_back(std::move(storage_key), value);
            return State<T>{std::move(value)};
        }
        return State<T>{std::any_cast<std::shared_ptr<T>>(it->second)};
    }

    void invalidate() const { detail::trigger_rebuild(); }

    float viewport_width() const noexcept {
        return detail::runtime().viewport_width;
    }

    float viewport_height() const noexcept {
        return detail::runtime().viewport_height;
    }

    bool compact_width(float breakpoint = 720.0f) const noexcept {
        auto width = viewport_width();
        return width > 0.0f && width < breakpoint;
    }
};

class View {
    std::function<void()> build_;

public:
    View() = default;

    template <typename F>
        requires std::invocable<F&>
              && (!std::same_as<std::decay_t<F>, View>)
    explicit View(F&& build) : build_(std::forward<F>(build)) {}

    void render() const {
        if (build_)
            build_();
    }

    View key(std::uint32_t) const { return *this; }
    View key(std::string_view) const { return *this; }
    View padding(SpaceToken) const { return *this; }
};

inline View as_view(View view) { return view; }

template <typename T>
concept Renderable = requires(T const& value) {
    value.render();
};

template <Renderable T>
inline View as_view(T value) {
    return View{[value = std::move(value)] {
        value.render();
    }};
}

enum class AxisAlignment {
    start,
    center,
    end,
    space_between,
};

inline CrossAxisAlignment cross_axis(AxisAlignment alignment) noexcept {
    switch (alignment) {
        case AxisAlignment::center: return CrossAxisAlignment::Center;
        case AxisAlignment::end: return CrossAxisAlignment::End;
        case AxisAlignment::start:
        case AxisAlignment::space_between:
        default: return CrossAxisAlignment::Start;
    }
}

inline MainAxisAlignment main_axis(AxisAlignment alignment) noexcept {
    switch (alignment) {
        case AxisAlignment::center: return MainAxisAlignment::Center;
        case AxisAlignment::end: return MainAxisAlignment::End;
        case AxisAlignment::space_between: return MainAxisAlignment::SpaceBetween;
        case AxisAlignment::start:
        default: return MainAxisAlignment::Start;
    }
}

struct StackOptions {
    SpaceToken spacing = SpaceToken::Md;
    AxisAlignment cross = AxisAlignment::start;
    AxisAlignment main = AxisAlignment::start;
};

inline detail::StackOptions to_detail(StackOptions options) noexcept {
    return detail::StackOptions{
        .spacing = options.spacing,
        .cross = cross_axis(options.cross),
        .main = main_axis(options.main),
    };
}

struct Text {
    std::string value;
    detail::Font size = detail::Font::Body;
    TextColor color_value = TextColor::Default;

    explicit Text(std::string text) : value(std::move(text)) {}
    explicit Text(char const* text) : value(text ? text : "") {}

    Text font(Font font_value) const {
        auto copy = *this;
        copy.size = to_detail(font_value);
        return copy;
    }

    Text color(TextColor next) const {
        auto copy = *this;
        copy.color_value = next;
        return copy;
    }

    void render() const {
        detail::Element element;
        element.kind = detail::ElementKind::Text;
        element.text = value;
        element.font = size;
        element.text_color = color_value;
        detail::append(std::move(element));
    }
};

struct Code {
    std::string value;

    explicit Code(std::string text) : value(std::move(text)) {}
    explicit Code(char const* text) : value(text ? text : "") {}

    void render() const {
        detail::Element element;
        element.kind = detail::ElementKind::Code;
        element.text = value;
        element.font = detail::Font::Code;
        detail::append(std::move(element));
    }
};

struct Link {
    std::string label;
    std::string href;

    Link(std::string label_text, std::string href_value)
        : label(std::move(label_text)), href(std::move(href_value)) {}

    void render() const {
        detail::Element element;
        element.kind = detail::ElementKind::Link;
        element.text = label;
        element.href = href;
        element.font = detail::Font::Body;
        element.text_color = TextColor::Accent;
        element.callback_id = detail::register_callback([href = href] {
            phenotype_open_url(href.data(), static_cast<unsigned int>(href.size()));
        });
        detail::append(std::move(element));
    }
};

struct Image {
    std::string url;
    float width = 0.0f;
    float height = 0.0f;

    Image(std::string image_url, float image_width, float image_height)
        : url(std::move(image_url)), width(image_width), height(image_height) {}

    void render() const {
        detail::Element element;
        element.kind = detail::ElementKind::Image;
        element.text = url;
        element.width = width;
        element.height = height;
        detail::append(std::move(element));
    }
};

struct Spacer {
    float height = 0.0f;
    explicit Spacer(float value) : height(value) {}

    void render() const {
        detail::Element element;
        element.kind = detail::ElementKind::Spacer;
        element.height = height;
        detail::append(std::move(element));
    }
};

struct Divider {
    void render() const {
        detail::Element element;
        element.kind = detail::ElementKind::Divider;
        detail::append(std::move(element));
    }
};

template <typename... Children>
View VStack(Children... children) {
    auto views = std::vector<View>{as_view(std::move(children))...};
    return View{[views = std::move(views)] {
        detail::append(detail::make_container(
            detail::ElementKind::VStack,
            detail::StackOptions{},
            [&] {
                for (auto const& view : views)
                    view.render();
            }));
    }};
}

template <typename... Children>
View VStack(StackOptions options, Children... children) {
    auto views = std::vector<View>{as_view(std::move(children))...};
    return View{[views = std::move(views), options] {
        detail::append(detail::make_container(
            detail::ElementKind::VStack,
            to_detail(options),
            [&] {
                for (auto const& view : views)
                    view.render();
            }));
    }};
}

template <typename... Children>
View HStack(Children... children) {
    auto views = std::vector<View>{as_view(std::move(children))...};
    return View{[views = std::move(views)] {
        detail::append(detail::make_container(
            detail::ElementKind::HStack,
            detail::StackOptions{.spacing = SpaceToken::Sm},
            [&] {
                for (auto const& view : views)
                    view.render();
            }));
    }};
}

template <typename... Children>
View HStack(StackOptions options, Children... children) {
    auto views = std::vector<View>{as_view(std::move(children))...};
    return View{[views = std::move(views), options] {
        detail::append(detail::make_container(
            detail::ElementKind::HStack,
            to_detail(options),
            [&] {
                for (auto const& view : views)
                    view.render();
            }));
    }};
}

template <typename Child>
View Weighted(float grow, Child child) {
    auto view = as_view(std::move(child));
    return View{[view = std::move(view), grow] {
        auto element = detail::make_container(
            detail::ElementKind::Weighted,
            detail::StackOptions{},
            [&] { view.render(); });
        element.weight = grow;
        detail::append(std::move(element));
    }};
}

template <typename... Children>
View Box(Children... children) {
    auto views = std::vector<View>{as_view(std::move(children))...};
    return View{[views = std::move(views)] {
        detail::append(detail::make_container(
            detail::ElementKind::Box,
            detail::StackOptions{},
            [&] {
                for (auto const& view : views)
                    view.render();
            }));
    }};
}

template <typename Child>
View Card(Child child) {
    auto view = as_view(std::move(child));
    return View{[view = std::move(view)] {
        detail::append(detail::make_container(
            detail::ElementKind::Card,
            detail::StackOptions{},
            [&] { view.render(); }));
    }};
}

template <typename Child>
View ScrollView(float height, Child child) {
    auto view = as_view(std::move(child));
    return View{[view = std::move(view), height] {
        auto element = detail::make_container(
            detail::ElementKind::ScrollView,
            detail::StackOptions{},
            [&] { view.render(); });
        element.height = height;
        detail::append(std::move(element));
    }};
}

enum class ButtonRole {
    normal,
    primary,
};

struct Button {
    std::string label;
    bool primary = false;
    bool enabled = true;
    float width_value = 0.0f;
    float height_value = -1.0f;
    std::function<void()> action;

    explicit Button(std::string text) : label(std::move(text)) {}
    explicit Button(char const* text) : label(text ? text : "") {}

    Button role(ButtonRole role_value) const {
        auto copy = *this;
        copy.primary = role_value == ButtonRole::primary;
        return copy;
    }

    Button disabled(bool value = true) const {
        auto copy = *this;
        copy.enabled = !value;
        return copy;
    }

    Button width(float value) const {
        auto copy = *this;
        copy.width_value = value;
        return copy;
    }

    Button height(float value) const {
        auto copy = *this;
        copy.height_value = value;
        return copy;
    }

    Button on_click(std::function<void()> callback) const {
        auto copy = *this;
        copy.action = std::move(callback);
        return copy;
    }

    void render() const {
        detail::Element element;
        element.kind = detail::ElementKind::Button;
        element.text = label;
        element.primary = primary;
        element.enabled = enabled;
        element.width = width_value;
        element.height = height_value;
        if (enabled) {
            auto callback = action;
            element.callback_id = detail::register_callback(
                [callback = std::move(callback)] {
                    auto generation = detail::runtime().generation;
                    if (callback)
                        callback();
                    if (generation == detail::runtime().generation)
                        detail::trigger_rebuild();
                });
        }
        detail::append(std::move(element));
    }
};

struct TextField {
    std::string placeholder;
    Binding<std::string> text;
    float width_value = 0.0f;
    bool enabled = true;
    bool error_value = false;
    std::string semantic;

    TextField(std::string placeholder_text, Binding<std::string> text_binding)
        : placeholder(std::move(placeholder_text)), text(std::move(text_binding)) {}

    TextField disabled(bool value = true) const {
        auto copy = *this;
        copy.enabled = !value;
        return copy;
    }

    TextField error(bool value = true) const {
        auto copy = *this;
        copy.error_value = value;
        return copy;
    }

    TextField width(float value) const {
        auto copy = *this;
        copy.width_value = value;
        return copy;
    }

    TextField semantic_label(char const* label) const {
        auto copy = *this;
        copy.semantic = label ? label : "";
        return copy;
    }

    void render() const {
        detail::Element element;
        element.kind = detail::ElementKind::TextField;
        element.text = placeholder;
        element.href = text.valid() ? text.get() : std::string{};
        element.width = width_value;
        element.enabled = enabled && text.valid();
        if (element.enabled) {
            auto binding = text;
            element.callback_id = detail::register_input(
                [binding] { return binding.valid() ? binding.get() : std::string{}; },
                [binding](std::string value) {
                    if (binding.valid())
                        binding.set(std::move(value));
                });
        }
        detail::append(std::move(element));
    }
};

struct Checkbox {
    std::string label;
    bool checked = false;
    std::function<void()> action;

    Checkbox(std::string text, bool value)
        : label(std::move(text)), checked(value) {}

    Checkbox on_toggle(std::function<void()> callback) const {
        auto copy = *this;
        copy.action = std::move(callback);
        return copy;
    }

    void render() const {
        detail::Element element;
        element.kind = detail::ElementKind::Checkbox;
        element.text = label;
        element.checked = checked;
        auto callback = action;
        element.callback_id = detail::register_callback([callback = std::move(callback)] {
            if (callback)
                callback();
        });
        detail::append(std::move(element));
    }
};

struct Radio {
    std::string label;
    bool selected = false;
    std::function<void()> action;

    Radio(std::string text, bool value)
        : label(std::move(text)), selected(value) {}

    Radio on_select(std::function<void()> callback) const {
        auto copy = *this;
        copy.action = std::move(callback);
        return copy;
    }

    void render() const {
        detail::Element element;
        element.kind = detail::ElementKind::Radio;
        element.text = label;
        element.checked = selected;
        auto callback = action;
        element.callback_id = detail::register_callback([callback = std::move(callback)] {
            if (callback)
                callback();
        });
        detail::append(std::move(element));
    }
};

struct Switch {
    std::string label;
    bool on = false;
    std::function<void()> action;

    Switch(std::string text, bool value)
        : label(std::move(text)), on(value) {}

    Switch on_toggle(std::function<void()> callback) const {
        auto copy = *this;
        copy.action = std::move(callback);
        return copy;
    }

    void render() const {
        detail::Element element;
        element.kind = detail::ElementKind::Switch;
        element.text = label;
        element.checked = on;
        auto callback = action;
        element.callback_id = detail::register_callback([callback = std::move(callback)] {
            if (callback)
                callback();
        });
        detail::append(std::move(element));
    }
};

struct Tabs {
    std::vector<std::string> items;
    std::size_t selected = 0;
    std::function<void(std::size_t)> action;

    Tabs(std::vector<std::string> tab_items, std::size_t selected_index)
        : items(std::move(tab_items)), selected(selected_index) {}

    Tabs on_select(std::function<void(std::size_t)> callback) const {
        auto copy = *this;
        copy.action = std::move(callback);
        return copy;
    }

    void render() const {
        detail::Element element;
        element.kind = detail::ElementKind::Tabs;
        element.items = items;
        element.selected = selected;
        for (std::size_t i = 0; i < items.size(); ++i) {
            auto callback = action;
            element.callback_ids.push_back(detail::register_callback(
                [callback = std::move(callback), i] {
                    if (callback)
                        callback(i);
                }));
        }
        detail::append(std::move(element));
    }
};

struct Progress {
    float value = 0.0f;
    float width_value = 220.0f;

    explicit Progress(float progress) : value(progress) {}

    Progress width(float value) const {
        auto copy = *this;
        copy.width_value = value;
        return copy;
    }

    void render() const {
        detail::Element element;
        element.kind = detail::ElementKind::Progress;
        element.progress = value;
        element.width = width_value;
        detail::append(std::move(element));
    }
};

template <typename Item, typename Key, typename Builder>
View ForEach(std::vector<Item> items, Key, Builder builder) {
    return View{[
        items = std::move(items),
        builder = std::move(builder)] {
        for (auto const& item : items)
            as_view(builder(item)).render();
    }};
}

template <typename App>
void run(App app = App{}) {
    auto store = std::make_shared<detail::StateStore>();
    auto context = std::make_shared<Context>(store);
    auto app_state = std::make_shared<App>(std::move(app));
    detail::runtime().runner = [context, app_state] {
        auto& rt = detail::runtime();
        rt.viewport_width = phenotype_get_canvas_width();
        rt.viewport_height = phenotype_get_canvas_height();
        rt.callbacks.clear();
        rt.inputs.clear();

        auto roots = std::vector<detail::Element>{};
        auto* previous = detail::current_children();
        detail::current_children() = &roots;
        as_view(app_state->body(*context)).render();
        detail::current_children() = previous;

        ++rt.generation;
        detail::render_elements(roots, rt.scroll_y);
    };
    detail::trigger_rebuild();
}

} // namespace ui

namespace layout {

template <typename F>
void row(F&& body,
         SpaceToken spacing = SpaceToken::Md,
         CrossAxisAlignment cross = CrossAxisAlignment::Start,
         MainAxisAlignment main = MainAxisAlignment::Start) {
    detail::append(detail::make_container(
        detail::ElementKind::HStack,
        detail::StackOptions{.spacing = spacing, .cross = cross, .main = main},
        std::forward<F>(body)));
}

template <typename F>
void column(F&& body,
            SpaceToken spacing = SpaceToken::Md,
            CrossAxisAlignment cross = CrossAxisAlignment::Start,
            MainAxisAlignment main = MainAxisAlignment::Start) {
    detail::append(detail::make_container(
        detail::ElementKind::VStack,
        detail::StackOptions{.spacing = spacing, .cross = cross, .main = main},
        std::forward<F>(body)));
}

template <typename F>
void sized_box(float width, F&& body) {
    auto element = detail::make_container(
        detail::ElementKind::SizedBox,
        detail::StackOptions{},
        std::forward<F>(body));
    element.width = width;
    detail::append(std::move(element));
}

template <typename F>
void card(F&& body) {
    detail::append(detail::make_container(
        detail::ElementKind::Card,
        detail::StackOptions{},
        std::forward<F>(body)));
}

inline void spacer(float height) {
    detail::Element element;
    element.kind = detail::ElementKind::Spacer;
    element.height = height;
    detail::append(std::move(element));
}

inline void divider() {
    detail::Element element;
    element.kind = detail::ElementKind::Divider;
    detail::append(std::move(element));
}

} // namespace layout

namespace widget {

inline void semantic_canvas(float width,
                            float height,
                            char const*,
                            std::function<void(Painter&)> paint_fn,
                            std::initializer_list<int> = {},
                            std::uint64_t = 0,
                            char const* = nullptr) {
    detail::Element element;
    element.kind = detail::ElementKind::Canvas;
    element.width = width;
    element.height = height;
    element.paint = std::move(paint_fn);
    detail::append(std::move(element));
}

} // namespace widget

namespace testing {

inline unsigned int callback_count() {
    return static_cast<unsigned int>(detail::runtime().callbacks.size());
}

inline void invoke_callback(unsigned int callback_id) {
    auto& callbacks = detail::runtime().callbacks;
    if (callback_id < callbacks.size())
        callbacks[callback_id]();
}

inline unsigned int input_count() {
    return static_cast<unsigned int>(detail::runtime().inputs.size());
}

inline void commit_focused_input(std::string value) {
    if (auto* input = detail::focused_input())
        input->set(std::move(value));
}

inline float total_height() {
    return detail::runtime().total_height;
}

inline bool command_buffer_contains(std::string_view needle) {
    auto haystack = std::string_view{
        reinterpret_cast<char const*>(phenotype_cmd_buf),
        phenotype_cmd_len,
    };
    return haystack.find(needle) != std::string_view::npos;
}

} // namespace testing

} // namespace phenotype

extern "C" {

PHENOTYPE_EXPORT("phenotype_get_cmd_buf")
unsigned char* phenotype_get_cmd_buf(void) {
    return phenotype_cmd_buf;
}

PHENOTYPE_EXPORT("phenotype_get_cmd_len")
unsigned int phenotype_get_cmd_len(void) {
    return phenotype_cmd_len;
}

PHENOTYPE_EXPORT("phenotype_repaint")
void phenotype_repaint(float scroll_y) {
    phenotype::detail::runtime().scroll_y = scroll_y;
    phenotype::detail::trigger_rebuild();
}

PHENOTYPE_EXPORT("phenotype_get_total_height")
float phenotype_get_total_height(void) {
    return phenotype::detail::runtime().total_height;
}

PHENOTYPE_EXPORT("phenotype_handle_event")
void phenotype_handle_event(unsigned int callback_id) {
    auto& rt = phenotype::detail::runtime();
    if (callback_id < rt.callbacks.size()) {
        auto generation = rt.generation;
        rt.callbacks[callback_id]();
        if (generation == rt.generation)
            phenotype::detail::trigger_rebuild();
    }
}

PHENOTYPE_EXPORT("phenotype_set_hover")
void phenotype_set_hover(unsigned int) {}

PHENOTYPE_EXPORT("phenotype_set_pointer")
unsigned int phenotype_set_pointer(float, float) {
    return 0u;
}

PHENOTYPE_EXPORT("phenotype_clear_pointer")
unsigned int phenotype_clear_pointer(void) {
    return 0u;
}

PHENOTYPE_EXPORT("phenotype_set_focus")
void phenotype_set_focus(unsigned int callback_id) {
    phenotype::detail::runtime().focused_id = callback_id;
    phenotype::detail::trigger_rebuild();
}

PHENOTYPE_EXPORT("phenotype_get_focused_id")
unsigned int phenotype_get_focused_id(void) {
    return phenotype::detail::runtime().focused_id;
}

PHENOTYPE_EXPORT("phenotype_handle_tab")
void phenotype_handle_tab(unsigned int) {}

PHENOTYPE_EXPORT("phenotype_toggle_caret")
void phenotype_toggle_caret(void) {}

PHENOTYPE_EXPORT("phenotype_handle_key")
void phenotype_handle_key(unsigned int, unsigned int) {}

inline constexpr unsigned int phenotype_input_buffer_size = 4096u;
alignas(4) unsigned char phenotype_input_buffer[phenotype_input_buffer_size] = {};
unsigned int phenotype_input_buffer_len = 0;

PHENOTYPE_EXPORT("phenotype_input_buf")
unsigned char* phenotype_input_buf(void) {
    return phenotype_input_buffer;
}

PHENOTYPE_EXPORT("phenotype_input_buf_size")
unsigned int phenotype_input_buf_size(void) {
    return phenotype_input_buffer_size;
}

PHENOTYPE_EXPORT("phenotype_input_buf_len")
unsigned int phenotype_input_buf_len(void) {
    return phenotype_input_buffer_len;
}

PHENOTYPE_EXPORT("phenotype_focused_is_input")
unsigned int phenotype_focused_is_input(void) {
    return phenotype::detail::focused_input() ? 1u : 0u;
}

PHENOTYPE_EXPORT("phenotype_input_load_focused")
unsigned int phenotype_input_load_focused(void) {
    auto* input = phenotype::detail::focused_input();
    if (!input) {
        phenotype_input_buffer_len = 0;
        return 0;
    }
    auto value = input->get();
    auto len = std::min<std::size_t>(value.size(), phenotype_input_buffer_size);
    std::memcpy(phenotype_input_buffer, value.data(), len);
    phenotype_input_buffer_len = static_cast<unsigned int>(len);
    return phenotype_input_buffer_len;
}

PHENOTYPE_EXPORT("phenotype_input_commit")
void phenotype_input_commit(unsigned int len) {
    if (len > phenotype_input_buffer_size)
        return;
    auto* input = phenotype::detail::focused_input();
    if (!input)
        return;
    input->set(std::string{
        reinterpret_cast<char const*>(phenotype_input_buffer),
        len,
    });
}

}

#undef PHENOTYPE_IMPORT
#undef PHENOTYPE_EXPORT
