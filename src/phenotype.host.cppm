// Capability concepts for host platform abstraction.
//
// The phenotype core (layout, paint, runner) is parameterized by these
// concepts so that it contains zero side effects. Each backend provides
// a concrete struct satisfying host_platform:
//
//   phenotype.wasm   — wasm_host  (JS shim imports)
//   phenotype.native — native_host (platform shell + text + renderer adapters)
//
// Tests inject null_host — no link-time mocking, no WASM runtime.

module;
#include <concepts>
export module phenotype.host;

import phenotype.types;

export namespace phenotype {

// ---- Capability concepts ----

// `text_measurer` returns a pixel width for a UTF-8 byte run rendered
// at `font_size` with the given `FontSpec`. Hosts that cannot resolve
// the named family fall back to the platform default (Core Text best
// match, DWrite IDWriteFontFallback, Android Typeface.create silent
// fallback) — measurements stay platform-consistent regardless.
template <class T>
concept text_measurer = requires(T const& t, float font_size,
                                  FontSpec font, char const* text,
                                  unsigned int len) {
    { t.measure_text(font_size, font, text, len) } -> std::same_as<float>;
};

template <class T>
concept render_backend = requires(T& t, unsigned int needed) {
    { t.buf()      } -> std::same_as<unsigned char*>;
    { t.buf_len()  } -> std::same_as<unsigned int&>;
    { t.buf_size() } -> std::same_as<unsigned int>;
    { t.ensure(needed) };
    { t.flush()    };
};

template <class T>
concept canvas_source = requires(T const& t) {
    { t.canvas_width()  } -> std::same_as<float>;
    { t.canvas_height() } -> std::same_as<float>;
};

template <class T>
concept url_opener = requires(T& t, char const* url, unsigned int len) {
    { t.open_url(url, len) };
};

template <class T>
concept host_platform = text_measurer<T> && render_backend<T>
                     && canvas_source<T> && url_opener<T>;

// ---- Test double ----

struct null_host {
    // text_measurer
    float measure_text(float font_size, FontSpec /*font*/,
                       char const* /*text*/, unsigned int len) const {
        return static_cast<float>(len) * font_size * 0.6f;
    }

    // render_backend
    static constexpr unsigned int SIZE = 65536;
    alignas(4) unsigned char buffer[SIZE]{};
    unsigned int len_ = 0;

    unsigned char* buf() { return buffer; }
    unsigned int& buf_len() { return len_; }
    unsigned int buf_size() { return SIZE; }
    void ensure(unsigned int needed) {
        if (len_ + needed > SIZE) flush();
    }
    void flush() { len_ = 0; }

    // canvas_source
    float canvas_width() const { return 800.0f; }
    float canvas_height() const { return 600.0f; }

    // url_opener
    void open_url(char const*, unsigned int) {}
};

static_assert(host_platform<null_host>);

} // namespace phenotype
