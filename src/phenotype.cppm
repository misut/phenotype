module;
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
export module phenotype;

export import phenotype.types;
export import phenotype.state;
export import phenotype.layout;
export import phenotype.paint;

// ============================================================
// WASM imports — JS shim implements these
// ============================================================

extern "C" __attribute__((import_module("phenotype"), import_name("flush")))
void phenotype_flush(void);

extern "C" __attribute__((import_module("phenotype"), import_name("get_canvas_width")))
float phenotype_get_canvas_width(void);

extern "C" __attribute__((import_module("phenotype"), import_name("get_canvas_height")))
float phenotype_get_canvas_height(void);

extern "C" __attribute__((import_module("phenotype"), import_name("open_url")))
void phenotype_open_url(char const* url, unsigned int len);

export namespace phenotype {

// ============================================================
// DSL Components
// ============================================================

// Text
inline void Text(str content) {
    auto* node = detail::alloc_node();
    node->text = std::string(content.data, content.len);
    node->font_size = detail::g_app.theme.body_font_size;
    node->text_color = detail::g_app.theme.foreground;

    if (Scope::current()) {
        node->style.text_align = Scope::current()->node->style.text_align;
        Scope::current()->node->children.push_back(node);
    }
}

// Button
inline void Button(str label, std::function<void()> on_click = {}) {
    auto* node = detail::alloc_node();
    node->text = std::string(label.data, label.len);
    node->font_size = detail::g_app.theme.body_font_size;
    node->text_color = detail::g_app.theme.foreground;
    node->background = detail::g_app.theme.code_bg;
    node->hover_background = detail::g_app.theme.border; // slightly darker on hover
    node->border_radius = 4;
    node->style.padding[0] = 6; node->style.padding[1] = 12;
    node->style.padding[2] = 6; node->style.padding[3] = 12;
    node->cursor_type = 1; // pointer

    if (on_click) {
        auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
        detail::g_app.callbacks.push_back(std::move(on_click));
        node->callback_id = id;
    }

    if (Scope::current())
        Scope::current()->node->children.push_back(node);
}

// Link
inline void Link(str label, str href) {
    auto* node = detail::alloc_node();
    node->text = std::string(label.data, label.len);
    node->font_size = detail::g_app.theme.small_font_size;
    node->text_color = detail::g_app.theme.accent;
    node->hover_text_color = {29, 78, 216, 255}; // darker blue on hover
    node->url = std::string(href.data, href.len);
    node->cursor_type = 1; // pointer

    // Register click callback to open URL
    auto url_copy = node->url;
    auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
    detail::g_app.callbacks.push_back([url_copy] {
        phenotype_open_url(url_copy.c_str(), static_cast<unsigned int>(url_copy.size()));
    });
    node->callback_id = id;

    if (Scope::current())
        Scope::current()->node->children.push_back(node);
}

// Code block
inline void Code(str content) {
    auto* node = detail::alloc_node();
    node->text = std::string(content.data, content.len);
    node->font_size = detail::g_app.theme.code_font_size;
    node->text_color = detail::g_app.theme.foreground;
    node->mono = true;
    node->background = detail::g_app.theme.code_bg;
    node->border_color = detail::g_app.theme.border;
    node->border_width = 1;
    node->border_radius = 6;
    node->style.padding[0] = 16; node->style.padding[1] = 16;
    node->style.padding[2] = 16; node->style.padding[3] = 16;

    if (Scope::current())
        Scope::current()->node->children.push_back(node);
}

// TextField — single-line text input bound to Trait<std::string>
inline void TextField(Trait<std::string>* state, str placeholder = "") {
    auto* node = detail::alloc_node();
    node->is_input = true;
    node->input_state = state;
    node->placeholder = std::string(placeholder.data, placeholder.len);

    // Display current value or placeholder
    auto const& val = state->value();
    node->text = val.empty() ? node->placeholder : val;
    node->font_size = detail::g_app.theme.body_font_size;
    node->text_color = val.empty() ? detail::g_app.theme.muted : detail::g_app.theme.foreground;
    node->background = {255, 255, 255, 255};
    node->border_color = detail::g_app.theme.border;
    node->border_width = 1;
    node->border_radius = 4;
    node->style.padding[0] = 8; node->style.padding[1] = 12;
    node->style.padding[2] = 8; node->style.padding[3] = 12;
    node->cursor_type = 1; // text cursor

    // Register callback (for focus) and track as input node
    auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
    detail::g_app.callbacks.push_back([]{}); // no-op click, focus handled by phenotype_set_focus
    node->callback_id = id;
    detail::g_app.input_nodes.push_back({id, node});

    if (Scope::current())
        Scope::current()->node->children.push_back(node);
}

// Divider
inline void Divider() {
    auto* node = detail::alloc_node();
    node->style.fixed_height = 1;
    node->background = detail::g_app.theme.border;

    if (Scope::current())
        Scope::current()->node->children.push_back(node);
}

// Spacer
inline void Spacer(unsigned int height_px) {
    auto* node = detail::alloc_node();
    node->style.fixed_height = static_cast<float>(height_px);

    if (Scope::current())
        Scope::current()->node->children.push_back(node);
}

// ============================================================
// Container helpers
// ============================================================

namespace detail {

inline void open_container(LayoutNode* container, std::function<void()> builder) {
    if (Scope::current())
        Scope::current()->node->children.push_back(container);
    Scope scope(container);
    auto* prev = Scope::current();
    Scope::set_current(&scope);
    builder();
    Scope::set_current(prev);
}

template<typename Arg>
void render_one(Arg&& arg) {
    if constexpr (std::is_invocable_v<Arg>) {
        arg();
    }
}

} // namespace detail

// Column
template<typename F>
    requires std::is_invocable_v<F>
void Column(F&& builder) {
    auto* node = detail::alloc_node();
    node->style.flex_direction = FlexDirection::Column;
    node->style.gap = 12;
    detail::open_container(node, std::forward<F>(builder));
}

template<typename A, typename B, typename... Rest>
void Column(A&& a, B&& b, Rest&&... rest) {
    auto* node = detail::alloc_node();
    node->style.flex_direction = FlexDirection::Column;
    node->style.gap = 12;
    if (Scope::current())
        Scope::current()->node->children.push_back(node);
    Scope child_scope(node);
    auto* prev = Scope::current();
    Scope::set_current(&child_scope);
    (detail::render_one(std::forward<A>(a)),
     detail::render_one(std::forward<B>(b)),
     (detail::render_one(std::forward<Rest>(rest)), ...));
    Scope::set_current(prev);
}

// Row
template<typename F>
    requires std::is_invocable_v<F>
void Row(F&& builder) {
    auto* node = detail::alloc_node();
    node->style.flex_direction = FlexDirection::Row;
    node->style.gap = 8;
    detail::open_container(node, std::forward<F>(builder));
}

template<typename A, typename B, typename... Rest>
void Row(A&& a, B&& b, Rest&&... rest) {
    auto* node = detail::alloc_node();
    node->style.flex_direction = FlexDirection::Row;
    node->style.gap = 8;
    if (Scope::current())
        Scope::current()->node->children.push_back(node);
    Scope child_scope(node);
    auto* prev = Scope::current();
    Scope::set_current(&child_scope);
    (detail::render_one(std::forward<A>(a)),
     detail::render_one(std::forward<B>(b)),
     (detail::render_one(std::forward<Rest>(rest)), ...));
    Scope::set_current(prev);
}

// Box
template<typename F>
    requires std::is_invocable_v<F>
void Box(F&& builder) {
    auto* node = detail::alloc_node();
    detail::open_container(node, std::forward<F>(builder));
}

template<typename A, typename B, typename... Rest>
void Box(A&& a, B&& b, Rest&&... rest) {
    auto* node = detail::alloc_node();
    if (Scope::current())
        Scope::current()->node->children.push_back(node);
    Scope child_scope(node);
    auto* prev = Scope::current();
    Scope::set_current(&child_scope);
    (detail::render_one(std::forward<A>(a)),
     detail::render_one(std::forward<B>(b)),
     (detail::render_one(std::forward<Rest>(rest)), ...));
    Scope::set_current(prev);
}

// ============================================================
// Compose-style higher-level components
// ============================================================

// Scaffold — page layout with topBar, content, bottomBar
inline void Scaffold(std::function<void()> top_bar,
                     std::function<void()> content,
                     std::function<void()> bottom_bar = {}) {
    if (top_bar) {
        auto* header = detail::alloc_node();
        header->style.flex_direction = FlexDirection::Column;
        header->style.gap = 8;
        header->style.cross_align = CrossAxisAlignment::Center;
        header->style.text_align = TextAlign::Center;
        header->background = detail::g_app.theme.hero_bg;
        header->style.padding[0] = 48; header->style.padding[1] = 24;
        header->style.padding[2] = 48; header->style.padding[3] = 24;

        detail::open_container(header, std::move(top_bar));
        // Post-build: apply hero styles to children
        auto const& t = detail::g_app.theme;
        if (header->children.size() >= 1) {
            header->children[0]->font_size = t.hero_title_size;
            header->children[0]->text_color = t.hero_fg;
        }
        if (header->children.size() >= 2) {
            header->children[1]->font_size = t.hero_subtitle_size;
            header->children[1]->text_color = t.hero_muted;
        }
    }

    if (content) {
        auto* main = detail::alloc_node();
        main->style.flex_direction = FlexDirection::Column;
        main->style.gap = 32;
        main->style.max_width = detail::g_app.theme.max_content_width;
        main->style.padding[0] = 32; main->style.padding[1] = 24;
        main->style.padding[2] = 32; main->style.padding[3] = 24;

        detail::open_container(main, std::move(content));
    }

    if (bottom_bar) {
        auto* footer = detail::alloc_node();
        footer->style.flex_direction = FlexDirection::Row;
        footer->style.gap = 0;
        footer->style.main_align = MainAxisAlignment::Center;
        footer->style.cross_align = CrossAxisAlignment::Center;
        footer->style.padding[0] = 32; footer->style.padding[1] = 24;
        footer->style.padding[2] = 32; footer->style.padding[3] = 24;
        footer->border_color = detail::g_app.theme.border;
        footer->border_width = 1;

        detail::open_container(footer, std::move(bottom_bar));
        // Post-build: apply footer styles to children
        auto const& t = detail::g_app.theme;
        for (auto* child : footer->children) {
            child->font_size = t.small_font_size;
            child->text_color = t.muted;
        }
    }
}

// Surface
template<typename F>
    requires std::is_invocable_v<F>
void Surface(F&& builder) {
    auto* node = detail::alloc_node();
    detail::open_container(node, std::forward<F>(builder));
}

// Card
template<typename F>
    requires std::is_invocable_v<F>
void Card(F&& builder) {
    auto* node = detail::alloc_node();
    node->border_radius = 8;
    node->background = {255, 255, 255, 255};
    node->style.padding[0] = 16; node->style.padding[1] = 16;
    node->style.padding[2] = 16; node->style.padding[3] = 16;
    detail::open_container(node, std::forward<F>(builder));
}

// ListItems
template<typename F>
    requires std::is_invocable_v<F>
void ListItems(F&& builder) {
    auto* node = detail::alloc_node();
    node->style.flex_direction = FlexDirection::Column;
    node->style.gap = 6;
    node->style.padding[3] = 24; // left indent for bullets
    detail::open_container(node, std::forward<F>(builder));
}

// Item (list item with bullet)
inline void Item(str content) {
    auto* row = detail::alloc_node();
    row->style.flex_direction = FlexDirection::Row;
    row->style.gap = 8;

    // Bullet
    auto* bullet = detail::alloc_node();
    bullet->text = "\xe2\x80\xa2"; // bullet character "•"
    bullet->font_size = detail::g_app.theme.body_font_size;
    bullet->text_color = detail::g_app.theme.foreground;
    row->children.push_back(bullet);

    // Text
    auto* text = detail::alloc_node();
    text->text = std::string(content.data, content.len);
    text->font_size = detail::g_app.theme.body_font_size;
    text->text_color = detail::g_app.theme.foreground;
    row->children.push_back(text);

    if (Scope::current())
        Scope::current()->node->children.push_back(row);
}

// ============================================================
// rebuild — full UI rebuild, shared by express() and Trait::set()
// ============================================================

namespace detail {

inline void rebuild() {
    auto& app = g_app;
    if (!app.app_builder) return;

    // Clear Trait subscribers (they point to old stack Scopes)
    for (auto& slot : app.states)
        if (slot.clearer) slot.clearer(slot.ptr);

    app.arena.reset();
    app.callbacks.clear();
    app.input_nodes.clear();
    app.encode_index = 0;

    auto* root = alloc_node();
    root->style.flex_direction = FlexDirection::Column;
    root->background = app.theme.background;
    app.root = root;

    Scope scope(root);
    Scope::set_current(&scope);
    app.app_builder();
    Scope::set_current(nullptr);

    float cw = phenotype_get_canvas_width();
    layout_node(root, cw);
    app.focusable_ids.clear();
    emit_clear(app.theme.background);
    float vh = phenotype_get_canvas_height();
    paint_node(root, 0, 0, app.scroll_y, vh);
    flush();
}

} // namespace detail

// ============================================================
// express() — application entry point
// ============================================================

template<typename F>
void express(F&& app_fn) {
    detail::g_app.app_builder = std::function<void()>(app_fn);
    detail::g_app.rebuild_fn = &detail::rebuild;
    detail::rebuild();
}

} // namespace phenotype

// ============================================================
// WASM exports — called from JS
// ============================================================

extern "C" {
    __attribute__((export_name("phenotype_repaint")))
    void phenotype_repaint(float scroll_y) {
        auto& app = phenotype::detail::g_app;
        app.scroll_y = scroll_y;
        if (!app.root) return;

        // Relayout only on resize (canvas width changed)
        float cw = phenotype_get_canvas_width();
        if (cw != app.root->width)
            phenotype::detail::layout_node(app.root, cw);

        app.focusable_ids.clear();
        phenotype::emit_clear(app.theme.background);
        float vh = phenotype_get_canvas_height();
        phenotype::detail::paint_node(app.root, 0, 0, scroll_y, vh);
        phenotype::flush();
    }

    __attribute__((export_name("phenotype_get_total_height")))
    float phenotype_get_total_height(void) {
        if (phenotype::detail::g_app.root)
            return phenotype::detail::g_app.root->height;
        return 0;
    }

    __attribute__((export_name("phenotype_handle_event")))
    void phenotype_handle_event(unsigned int callback_id) {
        if (callback_id < phenotype::detail::g_app.callbacks.size())
            phenotype::detail::g_app.callbacks[callback_id]();
    }

    __attribute__((export_name("phenotype_set_hover")))
    void phenotype_set_hover(unsigned int callback_id) {
        if (phenotype::detail::g_app.hovered_id == callback_id) return;
        phenotype::detail::g_app.hovered_id = callback_id;
        phenotype_repaint(phenotype::detail::g_app.scroll_y);
    }

    __attribute__((export_name("phenotype_set_focus")))
    void phenotype_set_focus(unsigned int callback_id) {
        phenotype::detail::g_app.focused_id = callback_id;
        phenotype::detail::g_app.caret_pos = 0xFFFFFFFF; // end of text
        phenotype::detail::g_app.caret_visible = true;
        phenotype_repaint(phenotype::detail::g_app.scroll_y);
    }

    __attribute__((export_name("phenotype_handle_tab")))
    void phenotype_handle_tab(unsigned int reverse) {
        auto& app = phenotype::detail::g_app;
        if (app.focusable_ids.empty()) return;
        int n = static_cast<int>(app.focusable_ids.size());
        // Find current index
        int idx = -1;
        for (int i = 0; i < n; ++i) {
            if (app.focusable_ids[i] == app.focused_id) { idx = i; break; }
        }
        if (reverse)
            idx = (idx <= 0) ? n - 1 : idx - 1;
        else
            idx = (idx < 0 || idx >= n - 1) ? 0 : idx + 1;
        app.focused_id = app.focusable_ids[idx];
        app.caret_visible = true;
        phenotype_repaint(app.scroll_y);
    }

    __attribute__((export_name("phenotype_toggle_caret")))
    void phenotype_toggle_caret() {
        auto& app = phenotype::detail::g_app;
        app.caret_visible = !app.caret_visible;
        phenotype_repaint(app.scroll_y);
    }

    __attribute__((export_name("phenotype_handle_key")))
    void phenotype_handle_key(unsigned int key_type, unsigned int codepoint) {
        auto& app = phenotype::detail::g_app;
        phenotype::LayoutNode* input_node = nullptr;
        for (auto& [id, node] : app.input_nodes) {
            if (id == app.focused_id) { input_node = node; break; }
        }
        if (!input_node || !input_node->input_state) return;

        auto* state = static_cast<phenotype::Trait<std::string>*>(input_node->input_state);
        auto val = state->value();

        switch (key_type) {
            case 0: { // character
                char buf[4];
                unsigned int len = 0;
                if (codepoint < 0x80) {
                    buf[0] = static_cast<char>(codepoint); len = 1;
                } else if (codepoint < 0x800) {
                    buf[0] = static_cast<char>(0xC0 | (codepoint >> 6));
                    buf[1] = static_cast<char>(0x80 | (codepoint & 0x3F)); len = 2;
                } else if (codepoint < 0x10000) {
                    buf[0] = static_cast<char>(0xE0 | (codepoint >> 12));
                    buf[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    buf[2] = static_cast<char>(0x80 | (codepoint & 0x3F)); len = 3;
                } else {
                    buf[0] = static_cast<char>(0xF0 | (codepoint >> 18));
                    buf[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                    buf[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    buf[3] = static_cast<char>(0x80 | (codepoint & 0x3F)); len = 4;
                }
                val.append(buf, len);
                break;
            }
            case 1: // backspace
                if (!val.empty()) {
                    // Remove last UTF-8 character
                    auto i = val.size() - 1;
                    while (i > 0 && (static_cast<unsigned char>(val[i]) & 0xC0) == 0x80) --i;
                    val.erase(i);
                }
                break;
            default:
                return; // other keys not yet handled
        }

        app.caret_visible = true; // reset blink on input
        state->set(std::move(val));
    }
}
