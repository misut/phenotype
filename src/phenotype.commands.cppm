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
struct MaterialRectCmd {
    float x, y, w, h;
    float radius;
    MaterialCommandDescriptor material;
};
// DrawTextCmd carries the decoded `Cmd::DrawText` payload. `mono`
// stays as a convenience boolean (still derived from flags bit 0)
// so existing pre-FontSpec consumers compile unchanged. New consumers
// read `family` / `weight` / `style` directly. Default-constructed
// values (empty family + Regular + Upright) reproduce the pre-
// FontSpec wire encoding bit-for-bit.
struct DrawTextCmd  {
    float x, y, font_size;
    bool mono;
    Color color;
    std::string text;
    std::string family;
    FontWeight weight = FontWeight::Regular;
    FontStyle  style  = FontStyle::Upright;
    // Radians, CCW about the canvas +Z axis, pivot at `(x, y)`.
    // Default 0.0f reproduces the pre-rotation wire encoding.
    float rotation = 0.0f;
    // Horizontal glyph stretch — see `FontSpec::width_factor`. 1.0f
    // is the native font advance; the macOS backend applies it via
    // a Core Text font matrix, Windows / Android round-trip it but
    // currently render axis-natural.
    float width_factor = 1.0f;
};
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

// Batched solid convex quadrilaterals. Each quad carries its own
// colour so CAD-style true-colour meshes can stay in one opcode while
// preserving per-primitive fill.
struct FillQuadsCmd { std::vector<PaintQuad> quads; };
struct FillRectsCmd { std::vector<PaintRect> rects; };

using DrawCommand = std::variant<
    ClearCmd, FillRectCmd, StrokeRectCmd, RoundRectCmd,
    DrawTextCmd, DrawLineCmd, HitRegionCmd, DrawImageCmd, ScissorCmd,
    DrawArcCmd, DrawPathCmd, FillPathCmd, FillQuadsCmd, FillRectsCmd,
    MaterialRectCmd>;

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
        case Cmd::MaterialRect: {
            float x = read_f32(), y = read_f32(), w = read_f32(), h = read_f32();
            float r = read_f32();
            auto kind = static_cast<MaterialKind>(read_u32());
            auto role = material_surface_role_from_wire(read_u32());
            float opacity = read_f32();
            float blur_radius = read_f32();
            auto tint = unpack(read_u32());
            float saturation = read_f32();
            float luminance_floor = read_f32();
            float luminance_gain = read_f32();
            float edge_highlight = read_f32();
            float edge_width = read_f32();
            float noise_opacity = read_f32();
            float shadow_alpha = read_f32();
            float shadow_radius = read_f32();
            out.emplace_back(MaterialRectCmd{
                x,
                y,
                w,
                h,
                r,
                MaterialCommandDescriptor{
                    kind,
                    role,
                    opacity,
                    blur_radius,
                    tint,
                    saturation,
                    luminance_floor,
                    luminance_gain,
                    edge_highlight,
                    edge_width,
                    noise_opacity,
                    shadow_alpha,
                    shadow_radius}});
            break;
        }
        case Cmd::DrawText: {
            float x = read_f32(), y = read_f32(), fs = read_f32();
            float rot = read_f32();
            float wf  = read_f32();
            unsigned int flags = read_u32();
            Color c = unpack(read_u32());
            unsigned int flen = read_u32();
            std::string family(reinterpret_cast<char const*>(&buf[pos]), flen);
            pos += flen;
            pos = (pos + 3) & ~3u;
            unsigned int tlen = read_u32();
            std::string text(reinterpret_cast<char const*>(&buf[pos]), tlen);
            pos += tlen;
            pos = (pos + 3) & ~3u;
            DrawTextCmd cmd{};
            cmd.x = x; cmd.y = y; cmd.font_size = fs;
            cmd.rotation = rot;
            cmd.width_factor = wf;
            cmd.mono = (flags & 1u) != 0;
            cmd.color = c;
            cmd.text = std::move(text);
            cmd.family = std::move(family);
            cmd.weight = (flags & 2u) ? FontWeight::Bold   : FontWeight::Regular;
            cmd.style  = (flags & 4u) ? FontStyle::Italic  : FontStyle::Upright;
            out.emplace_back(std::move(cmd));
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
        case Cmd::FillQuads: {
            unsigned int n = read_u32();
            std::vector<PaintQuad> quads;
            quads.reserve(n);
            for (unsigned int i = 0; i < n; ++i) {
                Color c = unpack(read_u32());
                float x0 = read_f32(), y0 = read_f32();
                float x1 = read_f32(), y1 = read_f32();
                float x2 = read_f32(), y2 = read_f32();
                float x3 = read_f32(), y3 = read_f32();
                quads.emplace_back(PaintQuad{
                    x0, y0, x1, y1, x2, y2, x3, y3, c});
            }
            out.emplace_back(FillQuadsCmd{std::move(quads)});
            break;
        }
        case Cmd::FillRects: {
            unsigned int n = read_u32();
            std::vector<PaintRect> rects;
            rects.reserve(n);
            for (unsigned int i = 0; i < n; ++i) {
                float x = read_f32(), y = read_f32();
                float w = read_f32(), h = read_f32();
                Color c = unpack(read_u32());
                rects.emplace_back(PaintRect{x, y, w, h, c});
            }
            out.emplace_back(FillRectsCmd{std::move(rects)});
            break;
        }
        default:
            return out;
        }
    }
    return out;
}

} // namespace phenotype
