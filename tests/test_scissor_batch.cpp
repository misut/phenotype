// test_scissor_batch — wire-format contract that the macOS Metal and
// Android Vulkan backends rely on to split each frame into ordered
// scissor batches.
//
// Backends accumulate decode-time pushes into a per-batch local
// vector, opening a new batch on Cmd::Scissor and coalescing
// back-to-back Scissors with no draws between them. They cannot batch
// correctly unless emit_scissor / emit_scissor_reset preserve their
// position in the command stream and round-trip x/y/w/h verbatim.
// This test pins that contract.

#include <cassert>
#include <cstdio>
#include <variant>
#include <vector>
import phenotype;

using namespace phenotype;

#if !defined(__wasi__) && !defined(__ANDROID__)
static null_host host;
#define CMD_BUF host.buf()
#define CMD_LEN host.buf_len()
#else
extern "C" {
    extern unsigned char phenotype_cmd_buf[];
    extern unsigned int phenotype_cmd_len;
    void phenotype_flush() {}
    float phenotype_measure_text(float fs, unsigned int, char const*, unsigned int len) {
        return static_cast<float>(len) * fs * 0.6f;
    }
    float phenotype_get_canvas_width() { return 800.0f; }
    float phenotype_get_canvas_height() { return 600.0f; }
    void phenotype_open_url(char const*, unsigned int) {}
}
#define CMD_BUF phenotype_cmd_buf
#define CMD_LEN phenotype_cmd_len
#endif

namespace {

void reset_buffer() {
#if !defined(__wasi__) && !defined(__ANDROID__)
    host.flush();
#else
    phenotype_cmd_len = 0;
#endif
}

Color const red{255, 0, 0, 255};

// ============================================================
// Wire-format pin: emit_scissor / reset round-trip exactly.
// ============================================================

void test_scissor_emits_with_exact_coords() {
    reset_buffer();
#if !defined(__wasi__) && !defined(__ANDROID__)
    emit_scissor(host, 50.0f, 60.0f, 200.0f, 100.0f);
#else
    emit_scissor(50.0f, 60.0f, 200.0f, 100.0f);
#endif
    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    assert(cmds.size() == 1);
    auto const* sc = std::get_if<ScissorCmd>(&cmds[0]);
    assert(sc != nullptr);
    assert(sc->x == 50.0f);
    assert(sc->y == 60.0f);
    assert(sc->w == 200.0f);
    assert(sc->h == 100.0f);
}

void test_scissor_reset_uses_zero_sentinel() {
    reset_buffer();
#if !defined(__wasi__) && !defined(__ANDROID__)
    emit_scissor_reset(host);
#else
    emit_scissor_reset();
#endif
    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    assert(cmds.size() == 1);
    auto const* sc = std::get_if<ScissorCmd>(&cmds[0]);
    assert(sc != nullptr);
    // Backends interpret (w==0 && h==0) as full-viewport restore.
    assert(sc->x == 0.0f);
    assert(sc->y == 0.0f);
    assert(sc->w == 0.0f);
    assert(sc->h == 0.0f);
}

// ============================================================
// Batch-split contract: an interleaved Fill -> Scissor -> Fill ->
// Fill -> Reset -> Fill stream parses with stable positions for
// each command. Backends use this ordering to assign every
// FillRect to the surrounding Scissor's batch.
// ============================================================

void test_scissor_interleaving_preserves_order() {
    reset_buffer();
#if !defined(__wasi__) && !defined(__ANDROID__)
    emit_fill_rect(host, 0.0f, 0.0f, 10.0f, 10.0f, red);
    emit_scissor(host, 50.0f, 50.0f, 200.0f, 200.0f);
    emit_fill_rect(host, 60.0f, 60.0f, 20.0f, 20.0f, red);
    emit_fill_rect(host, 70.0f, 70.0f, 20.0f, 20.0f, red);
    emit_scissor_reset(host);
    emit_fill_rect(host, 100.0f, 100.0f, 30.0f, 30.0f, red);
#else
    emit_fill_rect(0.0f, 0.0f, 10.0f, 10.0f, red);
    emit_scissor(50.0f, 50.0f, 200.0f, 200.0f);
    emit_fill_rect(60.0f, 60.0f, 20.0f, 20.0f, red);
    emit_fill_rect(70.0f, 70.0f, 20.0f, 20.0f, red);
    emit_scissor_reset();
    emit_fill_rect(100.0f, 100.0f, 30.0f, 30.0f, red);
#endif

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    assert(cmds.size() == 6);

    assert(std::holds_alternative<FillRectCmd>(cmds[0]));
    auto const* s1 = std::get_if<ScissorCmd>(&cmds[1]);
    assert(s1 != nullptr);
    assert(s1->x == 50.0f && s1->y == 50.0f
           && s1->w == 200.0f && s1->h == 200.0f);
    assert(std::holds_alternative<FillRectCmd>(cmds[2]));
    assert(std::holds_alternative<FillRectCmd>(cmds[3]));
    auto const* s2 = std::get_if<ScissorCmd>(&cmds[4]);
    assert(s2 != nullptr);
    assert(s2->x == 0.0f && s2->y == 0.0f
           && s2->w == 0.0f && s2->h == 0.0f);
    assert(std::holds_alternative<FillRectCmd>(cmds[5]));
}

// Backends coalesce a Cmd::Scissor that arrives while the current
// batch is empty by overwriting the rect in place. The wire format
// itself does not coalesce — both Scissor commands must reach the
// backend so the LATEST one wins. This pins that the wire keeps
// both back-to-back Scissors verbatim.
void test_back_to_back_scissors_are_both_emitted() {
    reset_buffer();
#if !defined(__wasi__) && !defined(__ANDROID__)
    emit_fill_rect(host, 0.0f, 0.0f, 10.0f, 10.0f, red);
    emit_scissor(host, 50.0f, 50.0f, 200.0f, 200.0f);
    emit_scissor(host, 10.0f, 10.0f, 100.0f, 100.0f);
    emit_fill_rect(host, 20.0f, 20.0f, 30.0f, 30.0f, red);
#else
    emit_fill_rect(0.0f, 0.0f, 10.0f, 10.0f, red);
    emit_scissor(50.0f, 50.0f, 200.0f, 200.0f);
    emit_scissor(10.0f, 10.0f, 100.0f, 100.0f);
    emit_fill_rect(20.0f, 20.0f, 30.0f, 30.0f, red);
#endif

    auto cmds = parse_commands(CMD_BUF, CMD_LEN);
    assert(cmds.size() == 4);
    assert(std::holds_alternative<FillRectCmd>(cmds[0]));
    auto const* s1 = std::get_if<ScissorCmd>(&cmds[1]);
    auto const* s2 = std::get_if<ScissorCmd>(&cmds[2]);
    assert(s1 != nullptr && s2 != nullptr);
    assert(s1->x == 50.0f && s2->x == 10.0f);
    assert(std::holds_alternative<FillRectCmd>(cmds[3]));
}

} // namespace

int main() {
    test_scissor_emits_with_exact_coords();
    test_scissor_reset_uses_zero_sentinel();
    test_scissor_interleaving_preserves_order();
    test_back_to_back_scissors_are_both_emitted();
    std::puts("\nAll scissor batch wire-format tests passed.");
    return 0;
}
