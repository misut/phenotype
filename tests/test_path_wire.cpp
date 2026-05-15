// test_path_wire — wire-format contract for Cmd::Path / Cmd::FillPath.
//
// Backends will CPU-flatten curve segments and re-dispatch them onto
// the existing line / arc instance buffers (stroke) or onto the fill
// triangle pipeline (fill). They rely on emit_stroke_path /
// emit_fill_path to round-trip a `PathBuilder` verb stream verbatim
// — verb tag, inline f32 payload, count, thickness, and color. This
// test pins that contract before any backend code reads it.

#include <cassert>
#include <cstdio>
#include <numbers>

// Native-only contract test; mirrors the scissor wire-format test.
#if !defined(__wasi__) && !defined(__ANDROID__)

#include <variant>
#include <vector>
import phenotype;

using namespace phenotype;

namespace {

null_host host;

void reset_buffer() { host.flush(); }

Color const red{255, 0, 0, 255};
Color const blue{0, 0, 255, 200};

constexpr float kPi = std::numbers::pi_v<float>;

// ============================================================
// Wire-format pin: emit_stroke_path round-trips empty / single /
// mixed verb streams, with thickness and color preserved.
// ============================================================

void test_empty_path_round_trips() {
    reset_buffer();
    PathBuilder p;
    emit_stroke_path(host, p, 1.5f, red);
    auto cmds = parse_commands(host.buf(), host.buf_len());
    assert(cmds.size() == 1);
    auto const* sp = std::get_if<DrawPathCmd>(&cmds[0]);
    assert(sp != nullptr);
    assert(sp->thickness == 1.5f);
    assert(sp->color.r == 255 && sp->color.g == 0
           && sp->color.b == 0 && sp->color.a == 255);
    assert(sp->segs.empty());
}

void test_single_move_round_trips() {
    reset_buffer();
    PathBuilder p;
    p.move_to(10.0f, 20.0f);
    emit_stroke_path(host, p, 2.0f, red);
    auto cmds = parse_commands(host.buf(), host.buf_len());
    assert(cmds.size() == 1);
    auto const* sp = std::get_if<DrawPathCmd>(&cmds[0]);
    assert(sp != nullptr);
    assert(sp->segs.size() == 1);
    auto const* mv = std::get_if<PathMoveTo>(&sp->segs[0]);
    assert(mv != nullptr);
    assert(mv->x == 10.0f && mv->y == 20.0f);
}

void test_mixed_path_round_trips_all_verbs() {
    reset_buffer();
    PathBuilder p;
    p.move_to(0.0f, 0.0f);
    p.line_to(10.0f, 0.0f);
    p.quad_to(15.0f, 5.0f, 20.0f, 0.0f);
    p.cubic_to(25.0f, -5.0f, 30.0f, 5.0f, 35.0f, 0.0f);
    p.arc_to(40.0f, 0.0f, 5.0f, 0.0f, kPi);
    p.close();
    emit_stroke_path(host, p, 1.0f, blue);

    auto cmds = parse_commands(host.buf(), host.buf_len());
    assert(cmds.size() == 1);
    auto const* sp = std::get_if<DrawPathCmd>(&cmds[0]);
    assert(sp != nullptr);
    assert(sp->thickness == 1.0f);
    assert(sp->color.r == 0 && sp->color.g == 0
           && sp->color.b == 255 && sp->color.a == 200);
    assert(sp->segs.size() == 6);

    auto const* mv = std::get_if<PathMoveTo>(&sp->segs[0]);
    assert(mv && mv->x == 0.0f && mv->y == 0.0f);
    auto const* ln = std::get_if<PathLineTo>(&sp->segs[1]);
    assert(ln && ln->x == 10.0f && ln->y == 0.0f);
    auto const* qd = std::get_if<PathQuadTo>(&sp->segs[2]);
    assert(qd && qd->cx == 15.0f && qd->cy == 5.0f
           && qd->x == 20.0f && qd->y == 0.0f);
    auto const* cb = std::get_if<PathCubicTo>(&sp->segs[3]);
    assert(cb && cb->c1x == 25.0f && cb->c1y == -5.0f
           && cb->c2x == 30.0f && cb->c2y == 5.0f
           && cb->x == 35.0f && cb->y == 0.0f);
    auto const* ar = std::get_if<PathArcTo>(&sp->segs[4]);
    assert(ar && ar->cx == 40.0f && ar->cy == 0.0f
           && ar->radius == 5.0f
           && ar->start_angle == 0.0f && ar->end_angle == kPi);
    assert(std::holds_alternative<PathClose>(sp->segs[5]));
}

// ============================================================
// FillPath uses a distinct opcode and skips the thickness slot —
// it must not be confused with DrawPath in the variant.
// ============================================================

void test_fill_path_distinct_from_stroke_path() {
    reset_buffer();
    PathBuilder p;
    p.move_to(0.0f, 0.0f);
    p.line_to(10.0f, 0.0f);
    p.line_to(10.0f, 10.0f);
    p.close();
    emit_fill_path(host, p, blue);

    auto cmds = parse_commands(host.buf(), host.buf_len());
    assert(cmds.size() == 1);
    auto const* fp = std::get_if<FillPathCmd>(&cmds[0]);
    assert(fp != nullptr);
    assert(fp->color.r == 0 && fp->color.g == 0
           && fp->color.b == 255 && fp->color.a == 200);
    assert(fp->segs.size() == 4);
    assert(std::holds_alternative<PathMoveTo>(fp->segs[0]));
    assert(std::holds_alternative<PathLineTo>(fp->segs[1]));
    assert(std::holds_alternative<PathLineTo>(fp->segs[2]));
    assert(std::holds_alternative<PathClose>(fp->segs[3]));

    // The same buffer must not parse as DrawPathCmd.
    assert(std::get_if<DrawPathCmd>(&cmds[0]) == nullptr);
}

// ============================================================
// Interleave with neighbouring opcodes — Scissor and DrawLine must
// preserve their position in the command stream relative to the
// path payloads. Backends rely on this to assign each path to the
// surrounding scissor batch.
// ============================================================

void test_path_interleaves_with_scissor_and_line() {
    reset_buffer();
    emit_draw_line(host, 0.0f, 0.0f, 10.0f, 10.0f, 1.0f, red);

    PathBuilder p1;
    p1.move_to(0.0f, 0.0f);
    p1.line_to(20.0f, 0.0f);
    emit_stroke_path(host, p1, 2.0f, red);

    emit_scissor(host, 5.0f, 5.0f, 100.0f, 100.0f);

    PathBuilder p2;
    p2.move_to(0.0f, 0.0f);
    p2.cubic_to(10.0f, 0.0f, 20.0f, 10.0f, 30.0f, 10.0f);
    emit_stroke_path(host, p2, 3.0f, blue);

    PathBuilder p3;
    p3.move_to(0.0f, 0.0f);
    p3.line_to(5.0f, 0.0f);
    p3.line_to(5.0f, 5.0f);
    p3.close();
    emit_fill_path(host, p3, blue);

    emit_scissor_reset(host);
    emit_draw_line(host, 50.0f, 50.0f, 60.0f, 60.0f, 1.0f, red);

    auto cmds = parse_commands(host.buf(), host.buf_len());
    assert(cmds.size() == 7);
    assert(std::holds_alternative<DrawLineCmd>(cmds[0]));
    assert(std::holds_alternative<DrawPathCmd>(cmds[1]));
    auto const* sc = std::get_if<ScissorCmd>(&cmds[2]);
    assert(sc != nullptr);
    assert(sc->x == 5.0f && sc->y == 5.0f
           && sc->w == 100.0f && sc->h == 100.0f);
    auto const* sp2 = std::get_if<DrawPathCmd>(&cmds[3]);
    assert(sp2 != nullptr);
    assert(sp2->thickness == 3.0f);
    assert(sp2->segs.size() == 2);
    assert(std::holds_alternative<FillPathCmd>(cmds[4]));
    auto const* sc2 = std::get_if<ScissorCmd>(&cmds[5]);
    assert(sc2 != nullptr);
    assert(sc2->w == 0.0f && sc2->h == 0.0f);
    assert(std::holds_alternative<DrawLineCmd>(cmds[6]));
}

// PathBuilder::translated offsets every coordinate word by (dx, dy)
// and passes through magnitude / angle words on ArcTo. Used by
// `Painter::stroke_path` adapters to convert canvas-local verbs into
// surface-local ones at emit time.
void test_translated_offsets_coordinates_only() {
    PathBuilder p;
    p.move_to(1.0f, 2.0f);
    p.line_to(3.0f, 4.0f);
    p.quad_to(5.0f, 6.0f, 7.0f, 8.0f);
    p.cubic_to(9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f);
    p.arc_to(15.0f, 16.0f, 5.0f, 0.0f, kPi);
    p.close();

    auto t = p.translated(100.0f, 200.0f);

    // Round-trip via the wire format to inspect typed segments.
    reset_buffer();
    emit_stroke_path(host, t, 1.0f, red);
    auto cmds = parse_commands(host.buf(), host.buf_len());
    assert(cmds.size() == 1);
    auto const* sp = std::get_if<DrawPathCmd>(&cmds[0]);
    assert(sp != nullptr);
    assert(sp->segs.size() == 6);

    auto const* mv = std::get_if<PathMoveTo>(&sp->segs[0]);
    assert(mv && mv->x == 101.0f && mv->y == 202.0f);
    auto const* ln = std::get_if<PathLineTo>(&sp->segs[1]);
    assert(ln && ln->x == 103.0f && ln->y == 204.0f);
    auto const* qd = std::get_if<PathQuadTo>(&sp->segs[2]);
    assert(qd && qd->cx == 105.0f && qd->cy == 206.0f
           && qd->x == 107.0f && qd->y == 208.0f);
    auto const* cb = std::get_if<PathCubicTo>(&sp->segs[3]);
    assert(cb && cb->c1x == 109.0f && cb->c1y == 210.0f
           && cb->c2x == 111.0f && cb->c2y == 212.0f
           && cb->x == 113.0f && cb->y == 214.0f);
    auto const* ar = std::get_if<PathArcTo>(&sp->segs[4]);
    assert(ar && ar->cx == 115.0f && ar->cy == 216.0f);
    // Radius and angles must NOT be translated.
    assert(ar->radius == 5.0f);
    assert(ar->start_angle == 0.0f && ar->end_angle == kPi);
    assert(std::holds_alternative<PathClose>(sp->segs[5]));

    // The original PathBuilder is unchanged.
    assert(p.verb_count == 6);
}

// Unknown verb tags abort decoding gracefully (consistent with the
// `Cmd default:` early-exit). Backends never see partially-parsed
// path commands.
void test_unknown_verb_aborts_decode() {
    reset_buffer();
    PathBuilder p;
    p.move_to(0.0f, 0.0f);
    // Inject an unknown verb tag (>= 100) directly into the verb
    // stream; emit_stroke_path will write it verbatim.
    p.verbs.push_back(0xFFu);
    ++p.verb_count;
    emit_stroke_path(host, p, 1.0f, red);

    auto cmds = parse_commands(host.buf(), host.buf_len());
    // The path command itself is not emplaced — parser bails on the
    // unknown verb and returns whatever it had already pushed (none).
    assert(cmds.empty());
}

void test_fill_quads_round_trips() {
    reset_buffer();
    PaintQuad quads[2] = {
        {0.0f, 0.0f, 10.0f, 0.0f, 10.0f, 10.0f, 0.0f, 10.0f, red},
        {20.0f, 1.0f, 30.0f, 2.0f, 29.0f, 12.0f, 19.0f, 11.0f, blue},
    };
    emit_fill_quads(host, quads, 2);

    auto cmds = parse_commands(host.buf(), host.buf_len());
    assert(cmds.size() == 1);
    auto const* fq = std::get_if<FillQuadsCmd>(&cmds[0]);
    assert(fq != nullptr);
    assert(fq->quads.size() == 2);
    assert(fq->quads[0].x0 == 0.0f && fq->quads[0].y0 == 0.0f);
    assert(fq->quads[0].x2 == 10.0f && fq->quads[0].y2 == 10.0f);
    assert(fq->quads[0].color == red);
    assert(fq->quads[1].x0 == 20.0f && fq->quads[1].y0 == 1.0f);
    assert(fq->quads[1].x3 == 19.0f && fq->quads[1].y3 == 11.0f);
    assert(fq->quads[1].color == blue);
}

void test_fill_quads_chunks_large_batches() {
    reset_buffer();
    std::vector<PaintQuad> quads;
    quads.reserve(1025);
    for (unsigned int i = 0; i < 1025; ++i) {
        float x = static_cast<float>(i);
        quads.push_back(PaintQuad{
            x, 0.0f,
            x + 1.0f, 0.0f,
            x + 1.0f, 1.0f,
            x, 1.0f,
            (i == 1024) ? blue : red});
    }

    emit_fill_quads(host, quads.data(), static_cast<unsigned int>(quads.size()));

    auto cmds = parse_commands(host.buf(), host.buf_len());
    assert(cmds.size() == 2);
    auto const* first = std::get_if<FillQuadsCmd>(&cmds[0]);
    auto const* second = std::get_if<FillQuadsCmd>(&cmds[1]);
    assert(first != nullptr && second != nullptr);
    assert(first->quads.size() == 1024);
    assert(second->quads.size() == 1);
    assert(first->quads.back().x0 == 1023.0f);
    assert(second->quads[0].x0 == 1024.0f);
    assert(second->quads[0].color == blue);
}

void test_fill_quads_interleaves_with_scissor() {
    reset_buffer();
    emit_scissor(host, 5.0f, 5.0f, 50.0f, 50.0f);
    PaintQuad quad{6.0f, 6.0f, 16.0f, 6.0f, 16.0f, 16.0f, 6.0f, 16.0f, red};
    emit_fill_quads(host, &quad, 1);
    emit_scissor_reset(host);
    emit_draw_line(host, 0.0f, 0.0f, 10.0f, 0.0f, 1.0f, blue);

    auto cmds = parse_commands(host.buf(), host.buf_len());
    assert(cmds.size() == 4);
    assert(std::holds_alternative<ScissorCmd>(cmds[0]));
    assert(std::holds_alternative<FillQuadsCmd>(cmds[1]));
    auto const* reset = std::get_if<ScissorCmd>(&cmds[2]);
    assert(reset != nullptr);
    assert(reset->w == 0.0f && reset->h == 0.0f);
    assert(std::holds_alternative<DrawLineCmd>(cmds[3]));
}

void test_fill_rects_round_trips() {
    reset_buffer();
    PaintRect rects[2] = {
        {1.0f, 2.0f, 3.0f, 4.0f, red},
        {5.0f, 6.0f, 7.0f, 8.0f, blue},
    };
    emit_fill_rects(host, rects, 2);

    auto cmds = parse_commands(host.buf(), host.buf_len());
    assert(cmds.size() == 1);
    auto const* fr = std::get_if<FillRectsCmd>(&cmds[0]);
    assert(fr != nullptr);
    assert(fr->rects.size() == 2);
    assert(fr->rects[0].x == 1.0f && fr->rects[0].h == 4.0f);
    assert(fr->rects[0].color == red);
    assert(fr->rects[1].y == 6.0f && fr->rects[1].w == 7.0f);
    assert(fr->rects[1].color == blue);
}

void test_linear_gradient_rect_round_trips() {
    reset_buffer();
    emit_linear_gradient_rect(
        host,
        4.0f,
        5.0f,
        -20.0f,
        10.0f,
        red,
        blue,
        GradientAxis::Horizontal,
        128);

    auto cmds = parse_commands(host.buf(), host.buf_len());
    assert(cmds.size() == 1);
    auto const* gradient = std::get_if<LinearGradientRectCmd>(&cmds[0]);
    assert(gradient != nullptr);
    assert(gradient->x == -16.0f);
    assert(gradient->y == 5.0f);
    assert(gradient->w == 20.0f);
    assert(gradient->h == 10.0f);
    assert(gradient->from == red);
    assert(gradient->to == blue);
    assert(gradient->axis == GradientAxis::Horizontal);
    assert(gradient->steps == max_linear_gradient_steps);
}

} // namespace

int main() {
    test_empty_path_round_trips();
    test_single_move_round_trips();
    test_mixed_path_round_trips_all_verbs();
    test_fill_path_distinct_from_stroke_path();
    test_path_interleaves_with_scissor_and_line();
    test_translated_offsets_coordinates_only();
    test_unknown_verb_aborts_decode();
    test_fill_quads_round_trips();
    test_fill_quads_chunks_large_batches();
    test_fill_quads_interleaves_with_scissor();
    test_fill_rects_round_trips();
    test_linear_gradient_rect_round_trips();
    std::puts("\nAll path wire-format tests passed.");
    return 0;
}

#else  // __wasi__ || __ANDROID__

int main() { return 0; }

#endif
