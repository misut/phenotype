// Command buffer parser — decodes the raw phenotype_cmd_buf bytes into
// typed C++ structs. The JS shim has an equivalent parseCommands() in
// shim/phenotype.js; this module is the C++ counterpart for native
// backends that consume draw commands directly.

module;
#include <cstring>
#include <string>
#include <variant>
#include <vector>
export module phenotype.commands;

import phenotype.types;

export namespace phenotype {

// ---- Typed draw commands (one per Cmd opcode) ----

struct ClearCmd     { Color color; };
struct FillRectCmd  { float x, y, w, h; Color color; };
struct StrokeRectCmd{ float x, y, w, h; float line_width; Color color; };
struct RoundRectCmd { float x, y, w, h; float radius; Color color; };
struct DrawTextCmd  { float x, y, font_size; bool mono; Color color; std::string text; };
struct DrawLineCmd  { float x1, y1, x2, y2; float thickness; Color color; };
struct HitRegionCmd { float x, y, w, h; unsigned int callback_id; unsigned int cursor_type; };
struct DrawImageCmd { float x, y, w, h; std::string url; };
// ScissorCmd { w == 0 && h == 0 } resets the scissor to the full
// viewport. A non-zero rect clips subsequent draw commands until the
// next ScissorCmd (backends do not support nested scissor).
struct ScissorCmd   { float x, y, w, h; };
// Arc swept CCW from `start_angle` to `end_angle` (radians, math /
// AutoCAD convention) around `(cx, cy)` at `radius`, stroked at
// `thickness`. Full circle: `start_angle = 0, end_angle = 2π`.
struct DrawArcCmd   { float cx, cy, radius, start_angle, end_angle, thickness; Color color; };

// ---- Path verb segments (used by DrawPathCmd / FillPathCmd) ----
//
// One PathSegment per emitted PathVerb. Backends visit the variant
// and dispatch onto the existing line / arc instance buffers (stroke)
// or feed a CPU ear-clip (fill). Coordinate convention follows the
// rest of the wire format — canvas-local pixels for `(x, y)` /
// control points, CCW math/AutoCAD radians for `ArcTo` angles.
struct PathMoveTo  { float x, y; };
struct PathLineTo  { float x, y; };
struct PathQuadTo  { float cx, cy, x, y; };
struct PathCubicTo { float c1x, c1y, c2x, c2y, x, y; };
struct PathArcTo   { float cx, cy, radius, start_angle, end_angle; };
struct PathClose   {};

using PathSegment = std::variant<
    PathMoveTo, PathLineTo, PathQuadTo, PathCubicTo, PathArcTo, PathClose>;

// Stroked path. `thickness` applies to every flattened line segment
// and to ArcTo segments (which the backend dispatches onto the
// existing arc SDF pipeline at the same width).
struct DrawPathCmd { float thickness; Color color; std::vector<PathSegment> segs; };

// Filled path. Single closed loop only — self-intersection / multi-
// loop / hole semantics are out of scope for this slab and land with
// HATCH support in a later slab.
struct FillPathCmd { Color color; std::vector<PathSegment> segs; };

using DrawCommand = std::variant<
    ClearCmd, FillRectCmd, StrokeRectCmd, RoundRectCmd,
    DrawTextCmd, DrawLineCmd, HitRegionCmd, DrawImageCmd, ScissorCmd,
    DrawArcCmd, DrawPathCmd, FillPathCmd>;

// ---- Parser ----

// Parse the raw command buffer into a flat list of typed commands.
// Pure function: takes a pointer + length (the same phenotype_cmd_buf
// that emit_* functions write to) and returns structured commands.
// Unknown opcodes terminate parsing early (returns what was parsed so
// far). Empty buffer returns an empty vector.
inline std::vector<DrawCommand> parse_commands(
        unsigned char const* buf, unsigned int len) {
    std::vector<DrawCommand> out;
    unsigned int pos = 0;

    auto read_u32 = [&]() -> unsigned int {
        unsigned int v;
        std::memcpy(&v, &buf[pos], 4);
        pos += 4;
        return v;
    };
    auto read_f32 = [&]() -> float {
        unsigned int bits = read_u32();
        float v;
        std::memcpy(&v, &bits, 4);
        return v;
    };
    auto unpack = [](unsigned int packed) -> Color {
        return {
            static_cast<unsigned char>((packed >> 24) & 0xFF),
            static_cast<unsigned char>((packed >> 16) & 0xFF),
            static_cast<unsigned char>((packed >>  8) & 0xFF),
            static_cast<unsigned char>( packed        & 0xFF),
        };
    };

    while (pos < len) {
        auto cmd = static_cast<Cmd>(read_u32());
        switch (cmd) {
        case Cmd::Clear:
            out.emplace_back(ClearCmd{unpack(read_u32())});
            break;
        case Cmd::FillRect: {
            float x = read_f32(), y = read_f32(), w = read_f32(), h = read_f32();
            out.emplace_back(FillRectCmd{x, y, w, h, unpack(read_u32())});
            break;
        }
        case Cmd::StrokeRect: {
            float x = read_f32(), y = read_f32(), w = read_f32(), h = read_f32();
            float lw = read_f32();
            out.emplace_back(StrokeRectCmd{x, y, w, h, lw, unpack(read_u32())});
            break;
        }
        case Cmd::RoundRect: {
            float x = read_f32(), y = read_f32(), w = read_f32(), h = read_f32();
            float r = read_f32();
            out.emplace_back(RoundRectCmd{x, y, w, h, r, unpack(read_u32())});
            break;
        }
        case Cmd::DrawText: {
            float x = read_f32(), y = read_f32(), fs = read_f32();
            unsigned int mono = read_u32();
            Color c = unpack(read_u32());
            unsigned int tlen = read_u32();
            std::string text(reinterpret_cast<char const*>(&buf[pos]), tlen);
            pos += tlen;
            pos = (pos + 3) & ~3u;
            out.emplace_back(DrawTextCmd{x, y, fs, mono != 0, c, std::move(text)});
            break;
        }
        case Cmd::DrawLine: {
            float x1 = read_f32(), y1 = read_f32();
            float x2 = read_f32(), y2 = read_f32();
            float th = read_f32();
            out.emplace_back(DrawLineCmd{x1, y1, x2, y2, th, unpack(read_u32())});
            break;
        }
        case Cmd::HitRegion: {
            float x = read_f32(), y = read_f32(), w = read_f32(), h = read_f32();
            unsigned int cb = read_u32(), ct = read_u32();
            out.emplace_back(HitRegionCmd{x, y, w, h, cb, ct});
            break;
        }
        case Cmd::DrawImage: {
            float x = read_f32(), y = read_f32(), w = read_f32(), h = read_f32();
            unsigned int ulen = read_u32();
            std::string url(reinterpret_cast<char const*>(&buf[pos]), ulen);
            pos += ulen;
            pos = (pos + 3) & ~3u;
            out.emplace_back(DrawImageCmd{x, y, w, h, std::move(url)});
            break;
        }
        case Cmd::Scissor: {
            float x = read_f32(), y = read_f32(), w = read_f32(), h = read_f32();
            out.emplace_back(ScissorCmd{x, y, w, h});
            break;
        }
        case Cmd::DrawArc: {
            float cx = read_f32(), cy = read_f32(), r = read_f32();
            float sa = read_f32(), ea = read_f32(), th = read_f32();
            out.emplace_back(DrawArcCmd{cx, cy, r, sa, ea, th, unpack(read_u32())});
            break;
        }
        case Cmd::Path: {
            float th = read_f32();
            Color c = unpack(read_u32());
            unsigned int n = read_u32();
            std::vector<PathSegment> segs;
            segs.reserve(n);
            for (unsigned int i = 0; i < n; ++i) {
                auto verb = static_cast<PathVerb>(read_u32());
                switch (verb) {
                case PathVerb::MoveTo: {
                    float x = read_f32(), y = read_f32();
                    segs.emplace_back(PathMoveTo{x, y});
                    break;
                }
                case PathVerb::LineTo: {
                    float x = read_f32(), y = read_f32();
                    segs.emplace_back(PathLineTo{x, y});
                    break;
                }
                case PathVerb::QuadTo: {
                    float cx = read_f32(), cy = read_f32();
                    float x = read_f32(), y = read_f32();
                    segs.emplace_back(PathQuadTo{cx, cy, x, y});
                    break;
                }
                case PathVerb::CubicTo: {
                    float c1x = read_f32(), c1y = read_f32();
                    float c2x = read_f32(), c2y = read_f32();
                    float x = read_f32(), y = read_f32();
                    segs.emplace_back(PathCubicTo{c1x, c1y, c2x, c2y, x, y});
                    break;
                }
                case PathVerb::ArcTo: {
                    float cx = read_f32(), cy = read_f32();
                    float r = read_f32();
                    float sa = read_f32(), ea = read_f32();
                    segs.emplace_back(PathArcTo{cx, cy, r, sa, ea});
                    break;
                }
                case PathVerb::Close:
                    segs.emplace_back(PathClose{});
                    break;
                default:
                    // Unknown verb — abort decoding the rest of the
                    // buffer (same policy as Cmd `default:` below).
                    return out;
                }
            }
            out.emplace_back(DrawPathCmd{th, c, std::move(segs)});
            break;
        }
        case Cmd::FillPath: {
            Color c = unpack(read_u32());
            unsigned int n = read_u32();
            std::vector<PathSegment> segs;
            segs.reserve(n);
            for (unsigned int i = 0; i < n; ++i) {
                auto verb = static_cast<PathVerb>(read_u32());
                switch (verb) {
                case PathVerb::MoveTo: {
                    float x = read_f32(), y = read_f32();
                    segs.emplace_back(PathMoveTo{x, y});
                    break;
                }
                case PathVerb::LineTo: {
                    float x = read_f32(), y = read_f32();
                    segs.emplace_back(PathLineTo{x, y});
                    break;
                }
                case PathVerb::QuadTo: {
                    float cx = read_f32(), cy = read_f32();
                    float x = read_f32(), y = read_f32();
                    segs.emplace_back(PathQuadTo{cx, cy, x, y});
                    break;
                }
                case PathVerb::CubicTo: {
                    float c1x = read_f32(), c1y = read_f32();
                    float c2x = read_f32(), c2y = read_f32();
                    float x = read_f32(), y = read_f32();
                    segs.emplace_back(PathCubicTo{c1x, c1y, c2x, c2y, x, y});
                    break;
                }
                case PathVerb::ArcTo: {
                    float cx = read_f32(), cy = read_f32();
                    float r = read_f32();
                    float sa = read_f32(), ea = read_f32();
                    segs.emplace_back(PathArcTo{cx, cy, r, sa, ea});
                    break;
                }
                case PathVerb::Close:
                    segs.emplace_back(PathClose{});
                    break;
                default:
                    return out;
                }
            }
            out.emplace_back(FillPathCmd{c, std::move(segs)});
            break;
        }
        default:
            return out;
        }
    }
    return out;
}

} // namespace phenotype
