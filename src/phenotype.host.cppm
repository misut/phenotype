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
#include <vector>
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
    // Vertical metrics for the resolved face at `font_size`. Hosts
    // without per-face metric resolution may return a zero
    // `FontMetrics{}` and let callers fall back to a font-size-based
    // heuristic — see `phenotype::FontMetrics` for the contract.
    { t.font_metrics(font_size, font) } -> std::same_as<FontMetrics>;
};

// `render_backend` exposes a command-stream buffer plus flush(). The
// buffer may be either fixed-size (wasm: a known global address shared
// with the JS reader) or growable (native hosts: a vector that
// `reserve()` extends on demand so single emits whose payload exceeds
// the current capacity don't have to be dropped). Overflow is detected
// and surfaced by `phenotype::detail::ensure` in phenotype.paint —
// backends do NOT define their own `ensure(needed)` member, since a
// parallel implementation would silently bypass the diagnostic path.
//
// `reserve(needed)` should grow the backing store so `buf_size()`
// returns at least `needed`; returns false when the host refuses to
// grow (wasm fixed buffer, or native cap exceeded). `ensure` falls
// through to the existing overflow-report path on false. Note that a
// successful `reserve` may invalidate any previously-cached `buf()`
// pointer — every emit_* in phenotype.paint already calls `r.buf()`
// per write, so this is safe in current call sites.
template <class T>
concept render_backend = requires(T& t, unsigned int needed) {
    { t.buf()             } -> std::same_as<unsigned char*>;
    { t.buf_len()         } -> std::same_as<unsigned int&>;
    { t.buf_size()        } -> std::same_as<unsigned int>;
    { t.flush()           };
    { t.reserve(needed)   } -> std::same_as<bool>;
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

    FontMetrics font_metrics(float /*font_size*/,
                             FontSpec /*font*/) const {
        return {};
    }

    // render_backend — growable command stream. Initial capacity
    // matches the original fixed-size buffer (65 536 bytes) so any
    // existing test that emits ≤ 65 536 bytes stays on the hot path
    // without reserve() ever firing. `reserve()` doubles past that,
    // capped at MAX_SIZE so a runaway emit can't blow up RSS.
    static constexpr unsigned int INIT_SIZE = 65536;
    static constexpr unsigned int MAX_SIZE  = 4 * 1024 * 1024;  // 4 MiB
    std::vector<unsigned char> buffer_ = std::vector<unsigned char>(INIT_SIZE);
    unsigned int len_ = 0;

    unsigned char* buf() { return buffer_.data(); }
    unsigned int& buf_len() { return len_; }
    unsigned int buf_size() { return static_cast<unsigned int>(buffer_.size()); }
    void flush() { len_ = 0; }
    [[nodiscard]] bool reserve(unsigned int needed) {
        if (needed > MAX_SIZE) return false;
        if (needed <= buffer_.size()) return true;
        std::size_t new_size = buffer_.size();
        while (new_size < needed) new_size *= 2;
        if (new_size > MAX_SIZE) new_size = MAX_SIZE;
        buffer_.resize(new_size);
        return needed <= buffer_.size();
    }

    // canvas_source
    float canvas_width() const { return 800.0f; }
    float canvas_height() const { return 600.0f; }

    // url_opener
    void open_url(char const*, unsigned int) {}
};

static_assert(host_platform<null_host>);

} // namespace phenotype
