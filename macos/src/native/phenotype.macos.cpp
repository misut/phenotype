#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include <webgpu/webgpu_cpp.h>

#include <phenotype/macos.hpp>

namespace {

constexpr size_t kMaxSymbolButtonCount = 128;
constexpr size_t kMaxPanelCount = 16;
constexpr size_t kMaxTextCount = 128;
constexpr uint32_t kTextAtlasPadding = 2;
constexpr uint32_t kTextAtlasRowAlignmentPixels = 64;

constexpr char kButtonShader[] = R"wgsl(
struct VertexOut {
    @builtin(position) position : vec4f,
    @location(0) pixel_position : vec2f,
};

struct SymbolButton {
    frame : vec4f,
    icon : vec4f,
    style : vec4f,
    control : vec4f,
    appearance : vec4f,
    color : vec4f,
    glyph : vec4f,
};

struct Panel {
    frame : vec4f,
    color : vec4f,
    style : vec4f,
};

struct TextRun {
    frame : vec4f,
    uv : vec4f,
    color : vec4f,
};

struct SceneUniforms {
    viewport : vec4f,
    counts : vec4f,
    panels : array<Panel, 16>,
    buttons : array<SymbolButton, 128>,
    texts : array<TextRun, 128>,
};

@group(0) @binding(0) var<uniform> scene : SceneUniforms;
@group(0) @binding(1) var text_atlas : texture_2d<f32>;
@group(0) @binding(2) var text_sampler : sampler;

@vertex
fn vertexMain(@builtin(vertex_index) index : u32) -> VertexOut {
    let clip = array<vec2f, 6>(
        vec2f(-1.0, -1.0),
        vec2f(-1.0,  1.0),
        vec2f( 1.0, -1.0),
        vec2f( 1.0, -1.0),
        vec2f(-1.0,  1.0),
        vec2f( 1.0,  1.0),
    );

    let viewport_size = scene.viewport.xy;
    let pixel_position = vec2f(
        (clip[index].x + 1.0) * 0.5 * viewport_size.x,
        (1.0 - clip[index].y) * 0.5 * viewport_size.y,
    );

    var out : VertexOut;
    out.position = vec4f(clip[index], 0.0, 1.0);
    out.pixel_position = pixel_position;
    return out;
}

fn roundedRectDistance(position : vec2f, half_size : vec2f, radius : f32) -> f32 {
    let q = abs(position) - (half_size - vec2f(radius));
    return length(max(q, vec2f(0.0))) + min(max(q.x, q.y), 0.0) - radius;
}

fn compositeOver(destination : vec4f, source_color : vec3f, source_alpha : f32) -> vec4f {
    let alpha = clamp(source_alpha, 0.0, 1.0);
    return vec4f(
        destination.rgb * (1.0 - alpha) + source_color * alpha,
        destination.a * (1.0 - alpha) + alpha,
    );
}

fn controlRadius(size : vec2f, shape : f32, ui_scale : f32) -> f32 {
    if (shape > 0.5) {
        return min(size.x, size.y) * 0.5;
    }
    return min(10.0 * ui_scale, min(size.x, size.y) * 0.5);
}

fn drawSymbolButton(layer : vec4f, pixel_position : vec2f, button : SymbolButton) -> vec4f {
    let ui_scale = max(scene.viewport.w, 1.0);
    let control_center = button.control.xy;
    let control_size = button.control.zw;
    let radius = controlRadius(control_size, button.appearance.x, ui_scale);
    let local_control_position = pixel_position - control_center;

    let control_edge = roundedRectDistance(local_control_position, control_size * 0.5, radius);
    let control_coverage = 1.0 - smoothstep(-1.0, 1.0, control_edge);
    let border_coverage = 1.0 - smoothstep(-1.0, 1.0, abs(control_edge) - (0.75 * ui_scale));
    let button_fill = vec3f(0.985, 0.988, 0.992);
    let button_border = vec3f(0.73, 0.76, 0.82);

    var out_layer = layer;
    if (button.appearance.y > 0.5) {
        out_layer = compositeOver(out_layer, button_fill, control_coverage * 0.72);
        out_layer = compositeOver(out_layer, button_border, border_coverage * control_coverage * 0.55);

        if (button.appearance.w > 0.5) {
            let divider_distance = abs(pixel_position.x - button.appearance.z) - (0.5 * ui_scale);
            let divider_coverage = 1.0 - smoothstep(-1.0, 1.0, divider_distance);
            let divider_height = max(0.0, control_size.y - (16.0 * ui_scale));
            let divider_y_distance = abs(local_control_position.y) - (divider_height * 0.5);
            let divider_y_coverage = 1.0 - smoothstep(-1.0, 1.0, divider_y_distance);
            out_layer = compositeOver(
                out_layer,
                button_border,
                divider_coverage * divider_y_coverage * control_coverage * 0.38,
            );
        }
    }

    let top_left = button.frame.xy - (button.frame.zw * 0.5);
    let local_position = pixel_position - top_left;
    let inside = step(0.0, local_position.x) *
        step(0.0, local_position.y) *
        step(local_position.x, button.frame.z) *
        step(local_position.y, button.frame.w);
    let local_uv = local_position / max(button.frame.zw, vec2f(1.0));
    let uv = button.icon.xy + ((button.icon.zw - button.icon.xy) * local_uv);
    let sample_alpha = textureSample(text_atlas, text_sampler, uv).a;
    return compositeOver(
        out_layer,
        button.color.rgb,
        button.color.a * sample_alpha * inside * control_coverage,
    );
}

fn drawPanel(layer : vec4f, pixel_position : vec2f, panel : Panel) -> vec4f {
    let ui_scale = max(scene.viewport.w, 1.0);
    let local_position = pixel_position - panel.frame.xy;
    let half_size = panel.frame.zw * 0.5;
    let radius = min(panel.style.x * ui_scale, min(half_size.x, half_size.y));
    let edge_distance = roundedRectDistance(local_position, half_size, radius);
    let coverage = 1.0 - smoothstep(-1.0, 1.0, edge_distance);
    return compositeOver(layer, panel.color.rgb, panel.color.a * coverage);
}

fn drawText(layer : vec4f, pixel_position : vec2f, text : TextRun) -> vec4f {
    let top_left = text.frame.xy - (text.frame.zw * 0.5);
    let local_position = pixel_position - top_left;
    let inside = step(0.0, local_position.x) *
        step(0.0, local_position.y) *
        step(local_position.x, text.frame.z) *
        step(local_position.y, text.frame.w);
    let local_uv = local_position / max(text.frame.zw, vec2f(1.0));
    let uv = text.uv.xy + ((text.uv.zw - text.uv.xy) * local_uv);
    let sample_alpha = textureSample(text_atlas, text_sampler, uv).a;
    return compositeOver(layer, text.color.rgb, text.color.a * sample_alpha * inside);
}

@fragment
fn fragmentMain(in : VertexOut) -> @location(0) vec4f {
    var layer = vec4f(0.0);
    let panel_count = min(u32(scene.counts.x), 16u);
    for (var index = 0u; index < panel_count; index = index + 1u) {
        layer = drawPanel(layer, in.pixel_position, scene.panels[index]);
    }

    let button_count = min(u32(scene.counts.y), 128u);
    for (var index = 0u; index < button_count; index = index + 1u) {
        layer = drawSymbolButton(layer, in.pixel_position, scene.buttons[index]);
    }

    let text_count = min(u32(scene.counts.z), 128u);
    for (var index = 0u; index < text_count; index = index + 1u) {
        layer = drawText(layer, in.pixel_position, scene.texts[index]);
    }
    return layer;
}
)wgsl";

struct SymbolButtonUniform {
  float frame[4];
  float icon[4];
  float style[4];
  float control[4];
  float appearance[4];
  float color[4];
  float glyph[4];
};

struct PanelUniform {
  float frame[4];
  float color[4];
  float style[4];
};

struct TextUniform {
  float frame[4];
  float uv[4];
  float color[4];
};

struct SceneUniforms {
  float viewport[4];
  float counts[4];
  PanelUniform panels[kMaxPanelCount];
  SymbolButtonUniform buttons[kMaxSymbolButtonCount];
  TextUniform texts[kMaxTextCount];
};

struct LayoutRect {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

struct LayoutWindowControls {
  bool has_leading_controls = false;
  LayoutRect leading_controls;
};

struct LayoutContext {
  LayoutWindowControls window_controls;
};

struct SymbolButtonLayout {
  LayoutRect frame;
  LayoutRect control_frame;
  phenotype::ui::Symbol symbol = phenotype::ui::Symbol::chevron_left;
  phenotype::ui::SymbolOptions options;
  phenotype::ui::ControlShape control_shape =
      phenotype::ui::ControlShape::square_circle;
  phenotype::ui::Color color = phenotype::ui::primary_label();
  bool is_enabled = true;
  bool draws_control = true;
  bool draws_divider = false;
  float divider_x = 0.0f;
};

struct PanelLayout {
  LayoutRect frame;
  phenotype::ui::Color color;
  float corner_radius = 0.0f;
};

struct TextLayout {
  LayoutRect frame;
  std::string content;
  phenotype::ui::Color color;
  float font_size = 17.0f;
  float font_weight = 400.0f;
  int line_limit = 0;
  phenotype::ui::TextOverflow overflow = phenotype::ui::TextOverflow::clip;
  phenotype::ui::TextTruncation truncation =
      phenotype::ui::TextTruncation::tail;
  bool centers_text = false;
};

struct HitTargetLayout {
  LayoutRect frame;
  std::function<void()> action;
  bool is_enabled = true;
};

struct SceneLayout {
  std::vector<PanelLayout> panels;
  std::vector<SymbolButtonLayout> buttons;
  std::vector<TextLayout> texts;
  std::vector<HitTargetLayout> hit_targets;
};

struct TextAtlasEntry {
  LayoutRect frame;
  float uv_left = 0.0f;
  float uv_top = 0.0f;
  float uv_right = 1.0f;
  float uv_bottom = 1.0f;
  phenotype::ui::Color color;
};

struct TextAtlas {
  uint32_t width = 1;
  uint32_t height = 1;
  std::vector<uint8_t> pixels = std::vector<uint8_t>(4, 0);
  std::vector<TextAtlasEntry> entries;
  std::vector<TextAtlasEntry> symbol_entries;
};

uint32_t PixelSize(CGFloat points, CGFloat scale) noexcept {
  double pixels = static_cast<double>(points * scale);
  if (pixels < 1.0) {
    return 1;
  }
  return static_cast<uint32_t>(pixels);
}

uint32_t AlignUp(uint32_t value, uint32_t alignment) noexcept {
  if (alignment == 0) {
    return value;
  }
  return ((value + alignment - 1) / alignment) * alignment;
}

constexpr uint32_t FontAxisTag(char a, char b, char c, char d) noexcept {
  return (static_cast<uint32_t>(a) << 24U) |
         (static_cast<uint32_t>(b) << 16U) |
         (static_cast<uint32_t>(c) << 8U) | static_cast<uint32_t>(d);
}

float LogicalPixel(CGFloat points, CGFloat scale) noexcept {
  return static_cast<float>(points * scale);
}

CGFloat ToNativeFontWeight(float weight) noexcept {
  if (weight >= 700.0f) {
    return NSFontWeightBold;
  }
  if (weight >= 600.0f) {
    return NSFontWeightSemibold;
  }
  if (weight >= 500.0f) {
    return NSFontWeightMedium;
  }
  if (weight <= 200.0f) {
    return NSFontWeightUltraLight;
  }
  if (weight <= 300.0f) {
    return NSFontWeightLight;
  }
  return NSFontWeightRegular;
}

NSFont *TextFont(float point_size, float weight) {
  CGFloat size = std::max<CGFloat>(1.0, point_size);
  return [NSFont systemFontOfSize:size weight:ToNativeFontWeight(weight)];
}

std::filesystem::path SourcePretendardFontPath() {
  std::filesystem::path source_file = __FILE__;
  return source_file.parent_path().parent_path().parent_path() / "resources" /
         "fonts" / "PretendardVariable.ttf";
}

std::filesystem::path SourceMaterialSymbolsFontPath() {
  std::filesystem::path source_file = __FILE__;
  return source_file.parent_path().parent_path().parent_path() / "resources" /
         "fonts" / "MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf";
}

std::filesystem::path FindPretendardFontPath() {
  std::vector<std::filesystem::path> candidates;

  NSBundle *bundle = [NSBundle mainBundle];
  NSString *resource_path = [bundle resourcePath];
  if (resource_path != nil) {
    candidates.emplace_back([resource_path UTF8String]);
    candidates.back() /= "Fonts/PretendardVariable.ttf";
  }

  std::filesystem::path current_path = std::filesystem::current_path();
  candidates.push_back(current_path / "macos" / "resources" / "fonts" /
                       "PretendardVariable.ttf");
  candidates.push_back(current_path / "../../../macos/resources/fonts/"
                                      "PretendardVariable.ttf");
  candidates.push_back(SourcePretendardFontPath());

  for (const std::filesystem::path &candidate : candidates) {
    std::error_code error;
    if (std::filesystem::is_regular_file(candidate, error)) {
      return candidate;
    }
  }
  return {};
}

std::filesystem::path FindMaterialSymbolsFontPath() {
  std::vector<std::filesystem::path> candidates;

  NSBundle *bundle = [NSBundle mainBundle];
  NSString *resource_path = [bundle resourcePath];
  if (resource_path != nil) {
    candidates.emplace_back([resource_path UTF8String]);
    candidates.back() /= "Fonts/MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf";
  }

  std::filesystem::path current_path = std::filesystem::current_path();
  candidates.push_back(
      current_path / "macos" / "resources" / "fonts" /
      "MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf");
  candidates.push_back(
      current_path / "../../../macos/resources/fonts/"
                     "MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf");
  candidates.push_back(SourceMaterialSymbolsFontPath());

  for (const std::filesystem::path &candidate : candidates) {
    std::error_code error;
    if (std::filesystem::is_regular_file(candidate, error)) {
      return candidate;
    }
  }
  return {};
}

void RegisterPretendardFontIfAvailable() {
  static std::once_flag once;
  std::call_once(once, [] {
    std::filesystem::path font_path = FindPretendardFontPath();
    if (font_path.empty()) {
      std::fprintf(stderr, "phenotype: Pretendard font file not found\n");
      return;
    }

    std::string font_path_text = font_path.string();
    NSString *native_path =
        [NSString stringWithUTF8String:font_path_text.c_str()];
    if (native_path == nil) {
      return;
    }

    NSURL *font_url = [NSURL fileURLWithPath:native_path];
    CFErrorRef error = nullptr;
    bool registered = CTFontManagerRegisterFontsForURL(
        reinterpret_cast<CFURLRef>(font_url), kCTFontManagerScopeProcess,
        &error);
    if (!registered && error != nullptr) {
      CFIndex code = CFErrorGetCode(error);
      if (code != kCTFontManagerErrorAlreadyRegistered) {
        std::fprintf(stderr,
                     "phenotype: failed to register Pretendard font (%ld)\n",
                     static_cast<long>(code));
      }
      CFRelease(error);
    }
  });
}

void RegisterMaterialSymbolsFontIfAvailable() {
  static std::once_flag once;
  std::call_once(once, [] {
    std::filesystem::path font_path = FindMaterialSymbolsFontPath();
    if (font_path.empty()) {
      std::fprintf(stderr, "phenotype: Material Symbols font file not found\n");
      return;
    }

    std::string font_path_text = font_path.string();
    NSString *native_path =
        [NSString stringWithUTF8String:font_path_text.c_str()];
    if (native_path == nil) {
      return;
    }

    NSURL *font_url = [NSURL fileURLWithPath:native_path];
    CFErrorRef error = nullptr;
    bool registered = CTFontManagerRegisterFontsForURL(
        reinterpret_cast<CFURLRef>(font_url), kCTFontManagerScopeProcess,
        &error);
    if (!registered && error != nullptr) {
      CFIndex code = CFErrorGetCode(error);
      if (code != kCTFontManagerErrorAlreadyRegistered) {
        std::fprintf(stderr,
                     "phenotype: failed to register Material Symbols font "
                     "(%ld)\n",
                     static_cast<long>(code));
      }
      CFRelease(error);
    }
  });
}

NSFont *DefaultTextFont(float point_size, float weight) {
  RegisterPretendardFontIfAvailable();

  CGFloat size = std::max<CGFloat>(1.0, point_size);
  NSDictionary *traits = @{
    NSFontWeightTrait : @(ToNativeFontWeight(weight)),
  };

  for (NSString *family_name in @[ @"Pretendard Variable", @"Pretendard" ]) {
    NSDictionary *attributes = @{
      NSFontFamilyAttribute : family_name,
      NSFontTraitsAttribute : traits,
    };
    NSFontDescriptor *descriptor =
        [NSFontDescriptor fontDescriptorWithFontAttributes:attributes];
    NSFont *font = [NSFont fontWithDescriptor:descriptor size:size];
    if (font != nil &&
        [[font familyName] rangeOfString:@"Pretendard"].location !=
            NSNotFound) {
      return font;
    }
  }

  return TextFont(point_size, weight);
}

NSFont *MaterialSymbolFont(float point_size,
                           phenotype::ui::SymbolOptions options) {
  RegisterMaterialSymbolsFontIfAvailable();

  CGFloat size = std::max<CGFloat>(1.0, point_size);
  NSDictionary *variations = @{
    @(FontAxisTag('F', 'I', 'L', 'L')) : @(options.fill ? 1.0 : 0.0),
    @(FontAxisTag('G', 'R', 'A', 'D')) :
        @(std::clamp(options.grade, -50.0f, 200.0f)),
    @(FontAxisTag('o', 'p', 's', 'z')) :
        @(std::clamp(options.optical_size, 20.0f, 48.0f)),
    @(FontAxisTag('w', 'g', 'h', 't')) :
        @(std::clamp(options.weight, 100.0f, 700.0f)),
  };
  NSDictionary *attributes = @{
    NSFontNameAttribute : @"MaterialSymbolsRounded-Regular",
    NSFontVariationAttribute : variations,
  };
  NSFontDescriptor *descriptor =
      [NSFontDescriptor fontDescriptorWithFontAttributes:attributes];
  NSFont *font = [NSFont fontWithDescriptor:descriptor size:size];
  if (font != nil) {
    return font;
  }
  return TextFont(point_size, options.weight);
}

std::string_view MaterialSymbolName(phenotype::ui::Symbol symbol) noexcept {
  switch (symbol) {
  case phenotype::ui::Symbol::chevron_left:
    return "chevron_left";
  case phenotype::ui::Symbol::chevron_right:
    return "chevron_right";
  case phenotype::ui::Symbol::folder:
    return "folder";
  case phenotype::ui::Symbol::description:
    return "description";
  }
  return "";
}

NSLineBreakMode
ToNativeLineBreakMode(phenotype::ui::TextOverflow overflow,
                      phenotype::ui::TextTruncation truncation) noexcept {
  if (overflow != phenotype::ui::TextOverflow::ellipsis) {
    return NSLineBreakByClipping;
  }

  switch (truncation) {
  case phenotype::ui::TextTruncation::head:
    return NSLineBreakByTruncatingHead;
  case phenotype::ui::TextTruncation::middle:
    return NSLineBreakByTruncatingMiddle;
  case phenotype::ui::TextTruncation::tail:
    return NSLineBreakByTruncatingTail;
  }
  return NSLineBreakByTruncatingTail;
}

phenotype::ui::Size MeasureText(std::string_view content, float font_size,
                                float font_weight) {
  if (content.empty()) {
    return {};
  }

  NSString *text = [NSString stringWithUTF8String:std::string(content).c_str()];
  if (text == nil) {
    return {};
  }

  NSFont *font = DefaultTextFont(font_size, font_weight);
  NSDictionary *attributes = @{NSFontAttributeName : font};
  NSSize size = [text sizeWithAttributes:attributes];
  return {
      static_cast<float>(std::ceil(size.width)),
      static_cast<float>(std::ceil(size.height)),
  };
}

TextAtlas BuildTextAtlas(const std::vector<TextLayout> &texts,
                         const std::vector<SymbolButtonLayout> &symbols,
                         float scale) {
  enum class PendingKind {
    text,
    symbol,
  };

  struct PendingText {
    PendingKind kind = PendingKind::text;
    size_t index = 0;
    LayoutRect frame;
    std::string content;
    phenotype::ui::Color color;
    float font_size = 17.0f;
    float font_weight = 400.0f;
    int line_limit = 0;
    phenotype::ui::TextOverflow overflow = phenotype::ui::TextOverflow::clip;
    phenotype::ui::TextTruncation truncation =
        phenotype::ui::TextTruncation::tail;
    phenotype::ui::SymbolOptions symbol_options;
    bool centers_text = false;
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
  };

  constexpr uint32_t max_atlas_width = 2048;
  float safe_scale = std::max(1.0f, scale);
  std::vector<PendingText> pending;
  pending.reserve(texts.size() + symbols.size());

  uint32_t cursor_x = kTextAtlasPadding;
  uint32_t cursor_y = kTextAtlasPadding;
  uint32_t row_height = 0;
  uint32_t atlas_width = 1;
  uint32_t atlas_height = 1;

  auto add_pending = [&](PendingText item) {
    if (item.content.empty() || item.frame.width <= 0.0f ||
        item.frame.height <= 0.0f) {
      return;
    }

    if (item.kind == PendingKind::text && item.index >= kMaxTextCount) {
      return;
    }
    if (item.kind == PendingKind::symbol &&
        item.index >= kMaxSymbolButtonCount) {
      return;
    }

    uint32_t width =
        std::max<uint32_t>(1, static_cast<uint32_t>(
                                  std::ceil(item.frame.width * safe_scale)));
    uint32_t height =
        std::max<uint32_t>(1, static_cast<uint32_t>(
                                  std::ceil(item.frame.height * safe_scale)));
    if (cursor_x + width + kTextAtlasPadding > max_atlas_width &&
        cursor_x > kTextAtlasPadding) {
      cursor_x = kTextAtlasPadding;
      cursor_y += row_height + kTextAtlasPadding;
      row_height = 0;
    }

    item.x = cursor_x;
    item.y = cursor_y;
    item.width = width;
    item.height = height;
    pending.push_back(std::move(item));

    cursor_x += width + kTextAtlasPadding;
    row_height = std::max(row_height, height);
    atlas_width = std::max(atlas_width, cursor_x);
    atlas_height =
        std::max(atlas_height, cursor_y + row_height + kTextAtlasPadding);
  };

  for (size_t index = 0; index < texts.size(); ++index) {
    const TextLayout &text = texts[index];
    if (pending.size() >= kMaxTextCount + kMaxSymbolButtonCount) {
      continue;
    }

    add_pending({
        .kind = PendingKind::text,
        .index = index,
        .frame = text.frame,
        .content = text.content,
        .color = text.color,
        .font_size = text.font_size,
        .font_weight = text.font_weight,
        .line_limit = text.line_limit,
        .overflow = text.overflow,
        .truncation = text.truncation,
        .centers_text = text.centers_text,
    });
  }

  for (size_t index = 0; index < symbols.size(); ++index) {
    if (pending.size() >= kMaxTextCount + kMaxSymbolButtonCount) {
      continue;
    }

    const SymbolButtonLayout &symbol = symbols[index];
    std::string_view symbol_name = MaterialSymbolName(symbol.symbol);
    add_pending({
        .kind = PendingKind::symbol,
        .index = index,
        .frame = symbol.frame,
        .content = std::string(symbol_name),
        .color = symbol.color,
        .font_size = symbol.options.optical_size,
        .font_weight = symbol.options.weight,
        .symbol_options = symbol.options,
        .centers_text = true,
    });
  }

  if (pending.empty()) {
    return {};
  }

  TextAtlas atlas;
  atlas.width = AlignUp(atlas_width, kTextAtlasRowAlignmentPixels);
  atlas.height = atlas_height;
  atlas.pixels.assign(static_cast<size_t>(atlas.width) * atlas.height * 4, 0);
  atlas.entries.reserve(pending.size());
  atlas.symbol_entries.resize(symbols.size());

  CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
  CGBitmapInfo bitmap_info =
      static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) |
      static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big);
  CGContextRef context =
      CGBitmapContextCreate(atlas.pixels.data(), atlas.width, atlas.height, 8,
                            atlas.width * 4, color_space, bitmap_info);
  CGColorSpaceRelease(color_space);
  if (context == nullptr) {
    return {};
  }

  CGContextTranslateCTM(context, 0.0, static_cast<CGFloat>(atlas.height));
  CGContextScaleCTM(context, 1.0, -1.0);

  NSGraphicsContext *graphics_context =
      [NSGraphicsContext graphicsContextWithCGContext:context flipped:YES];
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:graphics_context];

  for (const PendingText &text : pending) {
    NSString *content =
        [NSString stringWithUTF8String:text.content.c_str()];
    if (content == nil) {
      continue;
    }

    bool is_symbol = text.kind == PendingKind::symbol;
    NSFont *font =
        is_symbol
            ? MaterialSymbolFont(text.font_size * safe_scale,
                                 text.symbol_options)
            : DefaultTextFont(text.font_size * safe_scale, text.font_weight);
    NSMutableParagraphStyle *paragraph_style =
        [[NSMutableParagraphStyle alloc] init];
    [paragraph_style setAlignment:text.centers_text
                                      ? NSTextAlignmentCenter
                                      : NSTextAlignmentLeft];
    if (!is_symbol) {
      [paragraph_style setLineBreakMode:ToNativeLineBreakMode(
                                             text.overflow, text.truncation)];
    }
    NSDictionary *attributes = @{
      NSFontAttributeName : font,
      NSLigatureAttributeName : @1,
      NSParagraphStyleAttributeName : paragraph_style,
      NSForegroundColorAttributeName :
          [NSColor colorWithCalibratedWhite:1.0 alpha:1.0]
    };
    if (is_symbol) {
      NSSize size = [content sizeWithAttributes:attributes];
      CGFloat draw_x = static_cast<CGFloat>(text.x) +
                       (static_cast<CGFloat>(text.width) - size.width) * 0.5;
      CGFloat draw_y = static_cast<CGFloat>(text.y) +
                       (static_cast<CGFloat>(text.height) - size.height) * 0.5;
      [content drawAtPoint:NSMakePoint(draw_x, draw_y) withAttributes:attributes];
    } else {
      CGFloat draw_height = static_cast<CGFloat>(text.height);
      if (text.line_limit > 0) {
        CGFloat line_height =
            std::ceil([font ascender] - [font descender] + [font leading]);
        draw_height =
            std::min(draw_height,
                     line_height * static_cast<CGFloat>(text.line_limit));
      }
      NSStringDrawingOptions draw_options = NSStringDrawingUsesLineFragmentOrigin;
      if (text.overflow == phenotype::ui::TextOverflow::ellipsis) {
        draw_options |= NSStringDrawingTruncatesLastVisibleLine;
      }
      [content drawWithRect:NSMakeRect(text.x, text.y, text.width, draw_height)
                    options:draw_options
                 attributes:attributes];
    }
    [paragraph_style release];

    TextAtlasEntry entry{
        text.frame,
        static_cast<float>(text.x) / static_cast<float>(atlas.width),
        static_cast<float>(text.y) / static_cast<float>(atlas.height),
        static_cast<float>(text.x + text.width) /
            static_cast<float>(atlas.width),
        static_cast<float>(text.y + text.height) /
            static_cast<float>(atlas.height),
        text.color,
    };
    if (is_symbol) {
      if (text.index < atlas.symbol_entries.size()) {
        atlas.symbol_entries[text.index] = entry;
      }
    } else {
      atlas.entries.push_back(entry);
    }
  }

  [NSGraphicsContext restoreGraphicsState];
  CGContextRelease(context);
  return atlas;
}

constexpr std::array<NSWindowButton, 3> LeadingWindowButtonTypes() noexcept {
  return {
      NSWindowCloseButton,
      NSWindowMiniaturizeButton,
      NSWindowZoomButton,
  };
}

NSRect InitialWindowVisibleFrame(CGFloat width, CGFloat height) {
  NSArray<NSScreen *> *screens = [NSScreen screens];
  for (NSScreen *screen in screens) {
    NSRect frame = [screen visibleFrame];
    if (frame.origin.x == 0.0 && frame.origin.y == 0.0) {
      return frame;
    }
  }

  NSPoint mouse = [NSEvent mouseLocation];
  for (NSScreen *screen in screens) {
    if (NSPointInRect(mouse, [screen frame])) {
      return [screen visibleFrame];
    }
  }

  NSScreen *screen = [NSScreen mainScreen];
  if (screen) {
    return [screen visibleFrame];
  }
  return NSMakeRect(0.0, 0.0, width, height);
}

NSVisualEffectMaterial
ToNativeVisualMaterial(phenotype::macos::window::VisualMaterial material) {
  switch (material) {
  case phenotype::macos::window::VisualMaterial::under_window_background:
    return NSVisualEffectMaterialUnderWindowBackground;
  }
  return NSVisualEffectMaterialUnderWindowBackground;
}

void ApplyTitleBarStyle(NSWindow *window,
                        phenotype::macos::window::TitleBarStyle style) {
  if (style == phenotype::macos::window::TitleBarStyle::hidden) {
    [window
        setStyleMask:[window styleMask] | NSWindowStyleMaskFullSizeContentView];
    [window setTitleVisibility:NSWindowTitleHidden];
    [window setTitlebarAppearsTransparent:YES];
    [window setMovableByWindowBackground:NO];
    return;
  }

  [window setTitleVisibility:NSWindowTitleVisible];
  [window setTitlebarAppearsTransparent:NO];
  [window setMovableByWindowBackground:NO];
}

bool CaptureWindowControlFrames(NSWindow *window, std::array<NSRect, 3> &frames,
                                std::array<bool, 3> &has_frame) {
  frames.fill(NSZeroRect);
  has_frame.fill(false);
  if (!window) {
    return false;
  }

  bool has_any_frame = false;
  std::array<NSWindowButton, 3> button_types = LeadingWindowButtonTypes();
  for (size_t index = 0; index < button_types.size(); ++index) {
    NSButton *button = [window standardWindowButton:button_types[index]];
    if (!button || ![button superview]) {
      continue;
    }

    [[button superview] layoutSubtreeIfNeeded];
    frames[index] = [button frame];
    has_frame[index] = true;
    has_any_frame = true;
  }
  return has_any_frame;
}

bool NearlyEqual(CGFloat lhs, CGFloat rhs) noexcept {
  return std::abs(lhs - rhs) < 0.5;
}

bool NearlyEqual(NSRect lhs, NSRect rhs) noexcept {
  return NearlyEqual(lhs.origin.x, rhs.origin.x) &&
         NearlyEqual(lhs.origin.y, rhs.origin.y) &&
         NearlyEqual(lhs.size.width, rhs.size.width) &&
         NearlyEqual(lhs.size.height, rhs.size.height);
}

void ApplyWindowControlVerticalOffset(NSWindow *window,
                                      const std::array<NSRect, 3> &base_frames,
                                      const std::array<bool, 3> &has_base_frame,
                                      CGFloat offset) {
  if (!window) {
    return;
  }

  std::array<NSWindowButton, 3> button_types = LeadingWindowButtonTypes();
  for (size_t index = 0; index < button_types.size(); ++index) {
    if (!has_base_frame[index]) {
      continue;
    }

    NSButton *button = [window standardWindowButton:button_types[index]];
    if (!button || ![button superview]) {
      continue;
    }

    NSRect frame = base_frames[index];
    frame.origin.y -= offset;
    if (!NearlyEqual([button frame], frame)) {
      [button setFrame:frame];
    }
  }
}

LayoutContext BuildLayoutContext(NSWindow *window, NSView *content_view) {
  LayoutContext context;
  if (!window || !content_view) {
    return context;
  }

  bool has_controls = false;
  NSRect controls_rect = NSZeroRect;
  for (NSWindowButton button_type : LeadingWindowButtonTypes()) {
    NSButton *button = [window standardWindowButton:button_type];
    if (!button || [button isHidden] || ![button superview]) {
      continue;
    }

    NSRect button_rect = [button convertRect:[button bounds] toView:nil];
    button_rect = [content_view convertRect:button_rect fromView:nil];
    controls_rect =
        has_controls ? NSUnionRect(controls_rect, button_rect) : button_rect;
    has_controls = true;
  }

  if (!has_controls) {
    return context;
  }

  NSRect content_bounds = [content_view bounds];
  context.window_controls.has_leading_controls = true;
  context.window_controls.leading_controls = {
      static_cast<float>(NSMinX(controls_rect)),
      static_cast<float>(NSHeight(content_bounds) - NSMaxY(controls_rect)),
      static_cast<float>(NSWidth(controls_rect)),
      static_cast<float>(NSHeight(controls_rect)),
  };
  return context;
}

phenotype::ui::Size IntrinsicSize(const phenotype::ui::View &view) {
  namespace ui = phenotype::ui;

  if (view.preferred_size.width > 0.0f || view.preferred_size.height > 0.0f) {
    return view.preferred_size;
  }

  switch (view.kind) {
  case ui::ViewKind::button:
    return {40.0f, 36.0f};
  case ui::ViewKind::icon:
    return {view.symbol_options.optical_size, view.symbol_options.optical_size};
  case ui::ViewKind::text:
    return MeasureText(view.text_content, view.font_size_value,
                       view.font_weight_value);
  case ui::ViewKind::grid:
    return {view.grid_min_column_width, view.grid_row_height};
  case ui::ViewKind::spacer:
  case ui::ViewKind::empty:
    return {};
  case ui::ViewKind::button_group:
  case ui::ViewKind::panel:
  case ui::ViewKind::stack:
    break;
  }

  ui::Size size;
  bool has_visible_child = false;
  for (const ui::View &child : view.children) {
    ui::Size child_size = IntrinsicSize(child);
    if (view.axis == ui::LayoutAxis::horizontal) {
      if (has_visible_child) {
        size.width += view.child_spacing;
      }
      size.width += child_size.width;
      size.height = std::max(size.height, child_size.height);
    } else if (view.axis == ui::LayoutAxis::vertical) {
      if (has_visible_child) {
        size.height += view.child_spacing;
      }
      size.width = std::max(size.width, child_size.width);
      size.height += child_size.height;
    } else {
      size.width = std::max(size.width, child_size.width);
      size.height = std::max(size.height, child_size.height);
    }
    has_visible_child = true;
  }

  size.width += view.content_padding.left + view.content_padding.right;
  size.height += view.content_padding.top + view.content_padding.bottom;
  return size;
}

const phenotype::ui::View *FindIconContent(const phenotype::ui::View &view) {
  namespace ui = phenotype::ui;

  if (view.kind == ui::ViewKind::icon) {
    return &view;
  }
  for (const ui::View &child : view.children) {
    if (const ui::View *icon = FindIconContent(child)) {
      return icon;
    }
  }
  return nullptr;
}

LayoutRect ContentRect(const phenotype::ui::View &view, LayoutRect rect) {
  return {
      rect.x + view.content_padding.left,
      rect.y + view.content_padding.top,
      std::max(0.0f, rect.width - view.content_padding.left -
                         view.content_padding.right),
      std::max(0.0f, rect.height - view.content_padding.top -
                         view.content_padding.bottom),
  };
}

float ControlShapeValue(phenotype::ui::ControlShape shape) noexcept {
  switch (shape) {
  case phenotype::ui::ControlShape::square_circle:
    return 0.0f;
  case phenotype::ui::ControlShape::capsule:
    return 1.0f;
  }
  return 0.0f;
}

bool Contains(LayoutRect rect, phenotype::ui::Size point) noexcept {
  return point.width >= rect.x && point.width <= rect.x + rect.width &&
         point.height >= rect.y && point.height <= rect.y + rect.height;
}

void LayoutButtonGroup(const phenotype::ui::View &view, LayoutRect rect,
                       SceneLayout &scene) {
  namespace ui = phenotype::ui;

  LayoutRect content_rect = ContentRect(view, rect);
  float cursor_x = content_rect.x;
  bool draws_control = true;
  size_t visible_button_index = 0;
  for (const ui::View &child : view.children) {
    ui::Size child_size = IntrinsicSize(child);
    LayoutRect child_rect{cursor_x, content_rect.y, child_size.width,
                          child_size.height};

    if (child.kind == ui::ViewKind::button) {
      if (child.click_action && child_rect.width > 0.0f &&
          child_rect.height > 0.0f) {
        scene.hit_targets.push_back({
            child_rect,
            child.click_action,
            child.is_enabled,
        });
      }

      if (scene.buttons.size() >= kMaxSymbolButtonCount) {
        return;
      }
      const ui::View *icon = FindIconContent(child);
      if (icon) {
        bool draws_divider =
            visible_button_index == 0 && view.children.size() > 1;
        scene.buttons.push_back({
            child_rect,
            content_rect,
            icon->symbol,
            icon->symbol_options,
            view.control_shape,
            icon->foreground_color,
            child.is_enabled,
            draws_control,
            draws_divider,
            child_rect.x + child_rect.width + (view.child_spacing * 0.5f),
        });
        draws_control = false;
        ++visible_button_index;
      }
    }

    cursor_x += child_size.width + view.child_spacing;
  }
}

void LayoutView(const phenotype::ui::View &view, LayoutRect rect,
                const LayoutContext &context, SceneLayout &scene);

void LayoutGrid(const phenotype::ui::View &view, LayoutRect rect,
                const LayoutContext &context, SceneLayout &scene) {
  namespace ui = phenotype::ui;

  LayoutRect content_rect = ContentRect(view, rect);
  float min_column_width = std::max(1.0f, view.grid_min_column_width);
  float column_gap = std::max(0.0f, view.grid_column_gap);
  float row_gap = std::max(0.0f, view.grid_row_gap);
  size_t column_count = std::max<size_t>(
      1, static_cast<size_t>((content_rect.width + column_gap) /
                             (min_column_width + column_gap)));
  float total_gap = column_gap * static_cast<float>(column_count - 1);
  float cell_width =
      std::max(1.0f, (content_rect.width - total_gap) /
                         static_cast<float>(column_count));
  float row_height = std::max(1.0f, view.grid_row_height);

  for (size_t index = 0; index < view.children.size(); ++index) {
    size_t row = index / column_count;
    size_t column = index % column_count;
    LayoutRect child_rect{
        content_rect.x + (static_cast<float>(column) * (cell_width + column_gap)),
        content_rect.y + (static_cast<float>(row) * (row_height + row_gap)),
        cell_width,
        row_height,
    };
    LayoutView(view.children[index], child_rect, context, scene);
  }
}

void LayoutView(const phenotype::ui::View &view, LayoutRect rect,
                const LayoutContext &context, SceneLayout &scene) {
  namespace ui = phenotype::ui;

  if (view.leading_window_controls_placement.is_enabled &&
      context.window_controls.has_leading_controls) {
    const LayoutRect &controls = context.window_controls.leading_controls;
    ui::Size view_size = IntrinsicSize(view);
    rect.x =
        std::max(rect.x, controls.x + controls.width +
                             view.leading_window_controls_placement.spacing);
    if (view.leading_window_controls_placement.aligns_vertical_center) {
      rect.y =
          controls.y + (controls.height * 0.5f) - (view_size.height * 0.5f);
    }
  }

  if (view.click_action && rect.width > 0.0f && rect.height > 0.0f) {
    scene.hit_targets.push_back({
        rect,
        view.click_action,
        view.is_enabled,
    });
  }

  switch (view.kind) {
  case ui::ViewKind::empty:
  case ui::ViewKind::spacer:
    return;
  case ui::ViewKind::icon:
    if (scene.buttons.size() >= kMaxSymbolButtonCount) {
      return;
    }
    scene.buttons.push_back({
        rect,
        rect,
        view.symbol,
        view.symbol_options,
        view.control_shape,
        view.foreground_color,
        true,
        false,
        false,
        0.0f,
    });
    return;
  case ui::ViewKind::text:
    if (scene.texts.size() >= kMaxTextCount) {
      return;
    }
    scene.texts.push_back({
        rect,
        view.text_content,
        view.foreground_color,
        view.font_size_value,
        view.font_weight_value,
        view.text_line_limit,
        view.text_overflow,
        view.text_truncation,
        view.centers_text,
    });
    return;
  case ui::ViewKind::panel:
    if (scene.panels.size() >= kMaxPanelCount) {
      return;
    }
    scene.panels.push_back({
        rect,
        view.background_color,
        view.corner_radius_value,
    });
    return;
  case ui::ViewKind::button: {
    if (scene.buttons.size() >= kMaxSymbolButtonCount) {
      return;
    }
    const ui::View *icon = FindIconContent(view);
    if (!icon) {
      return;
    }
    scene.buttons.push_back({
        rect,
        rect,
        icon->symbol,
        icon->symbol_options,
        view.control_shape,
        icon->foreground_color,
        view.is_enabled,
        true,
        false,
        0.0f,
    });
    return;
  }
  case ui::ViewKind::button_group:
    LayoutButtonGroup(view, rect, scene);
    return;
  case ui::ViewKind::grid:
    LayoutGrid(view, rect, context, scene);
    return;
  case ui::ViewKind::stack:
    break;
  }

  LayoutRect content_rect = ContentRect(view, rect);

  if (view.axis == ui::LayoutAxis::overlay) {
    for (const ui::View &child : view.children) {
      LayoutView(child, content_rect, context, scene);
    }
    return;
  }

  float available_main =
      view.axis == ui::LayoutAxis::horizontal ? content_rect.width
                                              : content_rect.height;
  size_t flexible_child_count = 0;
  float fixed_main = view.children.empty()
                         ? 0.0f
                         : view.child_spacing *
                               static_cast<float>(view.children.size() - 1);

  for (const ui::View &child : view.children) {
    ui::Size child_size = IntrinsicSize(child);
    bool expands_on_axis =
        view.axis == ui::LayoutAxis::horizontal ? child.expands_width
                                                : child.expands_height;
    if (expands_on_axis) {
      ++flexible_child_count;
    } else {
      fixed_main += view.axis == ui::LayoutAxis::horizontal
                        ? child_size.width
                        : child_size.height;
    }
  }

  float flexible_main = 0.0f;
  if (flexible_child_count > 0) {
    flexible_main =
        std::max(0.0f, available_main - fixed_main) /
        static_cast<float>(flexible_child_count);
  }

  float cursor_x = content_rect.x;
  float cursor_y = content_rect.y;
  for (const ui::View &child : view.children) {
    ui::Size child_size = IntrinsicSize(child);
    if (view.axis == ui::LayoutAxis::horizontal) {
      if (child.expands_width) {
        child_size.width = flexible_main;
      }
      if (child.expands_height) {
        child_size.height = content_rect.height;
      }
    } else {
      if (child.expands_width) {
        child_size.width = content_rect.width;
      }
      if (child.expands_height) {
        child_size.height = flexible_main;
      }
    }

    float child_x = cursor_x;
    float child_y = cursor_y;
    if (view.axis == ui::LayoutAxis::horizontal && !child.expands_height) {
      child_y =
          content_rect.y + std::max(0.0f, content_rect.height -
                                              child_size.height) *
                               0.5f;
    } else if (view.axis == ui::LayoutAxis::vertical && view.centers_children &&
               !child.expands_width) {
      child_x =
          content_rect.x + std::max(0.0f, content_rect.width -
                                              child_size.width) *
                               0.5f;
    }

    LayoutRect child_rect{child_x, child_y, child_size.width,
                          child_size.height};
    LayoutView(child, child_rect, context, scene);
    if (view.axis == ui::LayoutAxis::horizontal) {
      cursor_x += child_size.width + view.child_spacing;
    } else {
      cursor_y += child_size.height + view.child_spacing;
    }
  }
}

SceneLayout LayoutScene(const phenotype::ui::View &root, float width,
                        float height, const LayoutContext &context) {
  SceneLayout scene;
  scene.panels.reserve(kMaxPanelCount);
  scene.buttons.reserve(kMaxSymbolButtonCount);
  scene.texts.reserve(kMaxTextCount);
  scene.hit_targets.reserve(64);
  LayoutView(root,
             {
                 0.0f,
                 0.0f,
                 width,
                 height,
             },
             context, scene);
  return scene;
}

SymbolButtonUniform MakeSymbolButton(const SymbolButtonLayout &button,
                                     const TextAtlasEntry &symbol,
                                     float scale) {
  float alpha_scale = button.is_enabled ? 1.0f : 0.44f;
  float safe_scale = std::max(1.0f, scale);

  return SymbolButtonUniform{
      {
          (button.frame.x + (button.frame.width * 0.5f)) * safe_scale,
          (button.frame.y + (button.frame.height * 0.5f)) * safe_scale,
          button.frame.width * safe_scale,
          button.frame.height * safe_scale,
      },
      {
          symbol.uv_left,
          symbol.uv_top,
          symbol.uv_right,
          symbol.uv_bottom,
      },
      {
          button.options.fill ? 1.0f : 0.0f,
          button.options.weight,
          button.options.grade,
          button.options.optical_size * safe_scale,
      },
      {
          (button.control_frame.x + (button.control_frame.width * 0.5f)) *
              safe_scale,
          (button.control_frame.y + (button.control_frame.height * 0.5f)) *
              safe_scale,
          button.control_frame.width * safe_scale,
          button.control_frame.height * safe_scale,
      },
      {
          ControlShapeValue(button.control_shape),
          button.draws_control ? 1.0f : 0.0f,
          button.divider_x * safe_scale,
          button.draws_divider ? 1.0f : 0.0f,
      },
      {
          button.color.red,
          button.color.green,
          button.color.blue,
          button.color.alpha * alpha_scale,
      },
      {
          0.0f,
          0.0f,
          0.0f,
          0.0f,
      },
  };
}

PanelUniform MakePanel(const PanelLayout &panel, float scale) {
  float safe_scale = std::max(1.0f, scale);
  return {
      {
          (panel.frame.x + (panel.frame.width * 0.5f)) * safe_scale,
          (panel.frame.y + (panel.frame.height * 0.5f)) * safe_scale,
          panel.frame.width * safe_scale,
          panel.frame.height * safe_scale,
      },
      {
          panel.color.red,
          panel.color.green,
          panel.color.blue,
          panel.color.alpha,
      },
      {
          panel.corner_radius,
          0.0f,
          0.0f,
          0.0f,
      },
  };
}

TextUniform MakeText(const TextAtlasEntry &text, float scale) {
  float safe_scale = std::max(1.0f, scale);
  return {
      {
          (text.frame.x + (text.frame.width * 0.5f)) * safe_scale,
          (text.frame.y + (text.frame.height * 0.5f)) * safe_scale,
          text.frame.width * safe_scale,
          text.frame.height * safe_scale,
      },
      {
          text.uv_left,
          text.uv_top,
          text.uv_right,
          text.uv_bottom,
      },
      {
          text.color.red,
          text.color.green,
          text.color.blue,
          text.color.alpha,
      },
  };
}

class DawnButtonRenderer {
public:
  bool Initialize(CAMetalLayer *layer, uint32_t width, uint32_t height,
                  float scale, phenotype::ui::Size layout_size,
                  LayoutContext layout_context, phenotype::ui::View root_view) {
    _root_view = std::move(root_view);
    _scale = scale;
    _layout_size = layout_size;
    _layout_context = layout_context;

    wgpu::InstanceFeatureName required_features[] = {
        wgpu::InstanceFeatureName::TimedWaitAny,
    };
    wgpu::InstanceDescriptor instance_descriptor;
    instance_descriptor.requiredFeatureCount = 1;
    instance_descriptor.requiredFeatures = required_features;

    _instance = wgpu::CreateInstance(&instance_descriptor);
    if (!_instance) {
      std::fprintf(stderr, "Dawn: failed to create WebGPU instance\n");
      return false;
    }

    wgpu::SurfaceSourceMetalLayer metal_layer_source;
    metal_layer_source.layer = layer;

    wgpu::SurfaceDescriptor surface_descriptor;
    surface_descriptor.nextInChain = &metal_layer_source;
    _surface = _instance.CreateSurface(&surface_descriptor);
    if (!_surface) {
      std::fprintf(stderr, "Dawn: failed to create Metal surface\n");
      return false;
    }

    if (!RequestAdapter() || !RequestDevice()) {
      return false;
    }

    CreateSceneUniformBuffer();
    CreateTextSampler();
    CreateTextTexture(1, 1);
    if (!ConfigureSurface(width, height)) {
      return false;
    }

    CreatePipeline();
    return static_cast<bool>(_pipeline);
  }

  void Resize(uint32_t width, uint32_t height, float scale,
              phenotype::ui::Size layout_size, LayoutContext layout_context) {
    if (width == 0 || height == 0 || !_device) {
      return;
    }
    _scale = scale;
    _layout_size = layout_size;
    _layout_context = layout_context;
    if (width == _width && height == _height) {
      UpdateSceneUniforms();
      return;
    }
    ConfigureSurface(width, height);
  }

  void UpdateRootView(phenotype::ui::View root_view) {
    _root_view = std::move(root_view);
    UpdateSceneUniforms();
  }

  bool ActivateAt(phenotype::ui::Size point) {
    SceneLayout scene = LayoutScene(_root_view, _layout_size.width,
                                    _layout_size.height, _layout_context);
    for (auto iterator = scene.hit_targets.rbegin();
         iterator != scene.hit_targets.rend(); ++iterator) {
      if (!iterator->is_enabled || !Contains(iterator->frame, point)) {
        continue;
      }
      iterator->action();
      return true;
    }
    return false;
  }

  void Render() {
    if (!_device || !_surface || !_pipeline) {
      return;
    }

    wgpu::SurfaceTexture surface_texture;
    _surface.GetCurrentTexture(&surface_texture);
    if (surface_texture.status !=
            wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
        surface_texture.status !=
            wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
      return;
    }

    wgpu::TextureView backbuffer = surface_texture.texture.CreateView();

    wgpu::RenderPassColorAttachment color_attachment;
    color_attachment.view = backbuffer;
    color_attachment.loadOp = wgpu::LoadOp::Clear;
    color_attachment.storeOp = wgpu::StoreOp::Store;
    color_attachment.clearValue = {0.0, 0.0, 0.0, 0.0};

    wgpu::RenderPassDescriptor render_pass_descriptor;
    render_pass_descriptor.colorAttachmentCount = 1;
    render_pass_descriptor.colorAttachments = &color_attachment;

    wgpu::CommandEncoder encoder = _device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass =
        encoder.BeginRenderPass(&render_pass_descriptor);
    pass.SetPipeline(_pipeline);
    pass.SetBindGroup(0, _scene_bind_group);
    pass.Draw(6);
    pass.End();

    wgpu::CommandBuffer commands = encoder.Finish();
    _device.GetQueue().Submit(1, &commands);
    _surface.Present();
    _instance.ProcessEvents();
  }

private:
  bool RequestAdapter() {
    wgpu::RequestAdapterOptions options;
    options.compatibleSurface = _surface;

    bool resolved = false;
    bool succeeded = false;
    wgpu::Future future = _instance.RequestAdapter(
        &options, wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter,
            wgpu::StringView message) {
          (void)message;
          resolved = true;
          succeeded = status == wgpu::RequestAdapterStatus::Success;
          if (succeeded) {
            _adapter = adapter;
          }
        });

    _instance.WaitAny(future, UINT64_MAX);
    if (!resolved || !succeeded || !_adapter) {
      std::fprintf(stderr, "Dawn: failed to request adapter\n");
      return false;
    }
    return true;
  }

  bool RequestDevice() {
    wgpu::DeviceDescriptor descriptor;
    descriptor.SetUncapturedErrorCallback([](const wgpu::Device &,
                                             wgpu::ErrorType error_type,
                                             wgpu::StringView message) {
      std::fprintf(stderr, "Dawn device error (%u): %.*s\n",
                   static_cast<unsigned>(error_type),
                   static_cast<int>(message.length), message.data);
    });

    bool resolved = false;
    bool succeeded = false;
    wgpu::Future future = _adapter.RequestDevice(
        &descriptor, wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestDeviceStatus status, wgpu::Device device,
            wgpu::StringView message) {
          (void)message;
          resolved = true;
          succeeded = status == wgpu::RequestDeviceStatus::Success;
          if (succeeded) {
            _device = device;
          }
        });

    _instance.WaitAny(future, UINT64_MAX);
    if (!resolved || !succeeded || !_device) {
      std::fprintf(stderr, "Dawn: failed to request device\n");
      return false;
    }
    return true;
  }

  bool ConfigureSurface(uint32_t width, uint32_t height) {
    wgpu::SurfaceCapabilities capabilities;
    _surface.GetCapabilities(_adapter, &capabilities);
    if (capabilities.formatCount == 0) {
      std::fprintf(stderr, "Dawn: surface returned no supported formats\n");
      return false;
    }

    _format = capabilities.formats[0];
    _width = width;
    _height = height;
    UpdateSceneUniforms();

    wgpu::SurfaceConfiguration configuration;
    configuration.device = _device;
    configuration.format = _format;
    configuration.usage = wgpu::TextureUsage::RenderAttachment;
    configuration.width = _width;
    configuration.height = _height;
    configuration.presentMode = wgpu::PresentMode::Fifo;
    configuration.alphaMode = wgpu::CompositeAlphaMode::Premultiplied;
    _surface.Configure(&configuration);
    return true;
  }

  void CreatePipeline() {
    wgpu::ShaderSourceWGSL wgsl;
    wgsl.code = kButtonShader;

    std::array<wgpu::BindGroupLayoutEntry, 3> bind_group_layout_entries{};
    bind_group_layout_entries[0].binding = 0;
    bind_group_layout_entries[0].visibility =
        wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    bind_group_layout_entries[0].buffer.type =
        wgpu::BufferBindingType::Uniform;
    bind_group_layout_entries[0].buffer.minBindingSize =
        sizeof(SceneUniforms);

    bind_group_layout_entries[1].binding = 1;
    bind_group_layout_entries[1].visibility = wgpu::ShaderStage::Fragment;
    bind_group_layout_entries[1].texture.sampleType =
        wgpu::TextureSampleType::Float;
    bind_group_layout_entries[1].texture.viewDimension =
        wgpu::TextureViewDimension::e2D;

    bind_group_layout_entries[2].binding = 2;
    bind_group_layout_entries[2].visibility = wgpu::ShaderStage::Fragment;
    bind_group_layout_entries[2].sampler.type =
        wgpu::SamplerBindingType::Filtering;

    wgpu::BindGroupLayoutDescriptor bind_group_layout_descriptor;
    bind_group_layout_descriptor.entryCount = bind_group_layout_entries.size();
    bind_group_layout_descriptor.entries = bind_group_layout_entries.data();
    _bind_group_layout =
        _device.CreateBindGroupLayout(&bind_group_layout_descriptor);

    wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor;
    pipeline_layout_descriptor.bindGroupLayoutCount = 1;
    pipeline_layout_descriptor.bindGroupLayouts = &_bind_group_layout;
    wgpu::PipelineLayout pipeline_layout =
        _device.CreatePipelineLayout(&pipeline_layout_descriptor);

    wgpu::ShaderModuleDescriptor shader_descriptor;
    shader_descriptor.nextInChain = &wgsl;
    wgpu::ShaderModule shader = _device.CreateShaderModule(&shader_descriptor);

    wgpu::ColorTargetState color_target;
    color_target.format = _format;
    color_target.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragment;
    fragment.module = shader;
    fragment.entryPoint = "fragmentMain";
    fragment.targetCount = 1;
    fragment.targets = &color_target;

    wgpu::RenderPipelineDescriptor pipeline_descriptor;
    pipeline_descriptor.layout = pipeline_layout;
    pipeline_descriptor.vertex.module = shader;
    pipeline_descriptor.vertex.entryPoint = "vertexMain";
    pipeline_descriptor.primitive.topology =
        wgpu::PrimitiveTopology::TriangleList;
    pipeline_descriptor.fragment = &fragment;
    pipeline_descriptor.multisample.count = 1;

    _pipeline = _device.CreateRenderPipeline(&pipeline_descriptor);
    CreateSceneBindGroup();
  }

  void CreateSceneBindGroup() {
    if (!_bind_group_layout || !_scene_uniform_buffer || !_text_texture_view ||
        !_text_sampler) {
      return;
    }

    std::array<wgpu::BindGroupEntry, 3> bind_group_entries{};
    bind_group_entries[0].binding = 0;
    bind_group_entries[0].buffer = _scene_uniform_buffer;
    bind_group_entries[0].offset = 0;
    bind_group_entries[0].size = sizeof(SceneUniforms);

    bind_group_entries[1].binding = 1;
    bind_group_entries[1].textureView = _text_texture_view;

    bind_group_entries[2].binding = 2;
    bind_group_entries[2].sampler = _text_sampler;

    wgpu::BindGroupDescriptor bind_group_descriptor;
    bind_group_descriptor.layout = _bind_group_layout;
    bind_group_descriptor.entryCount = bind_group_entries.size();
    bind_group_descriptor.entries = bind_group_entries.data();
    _scene_bind_group = _device.CreateBindGroup(&bind_group_descriptor);
  }

  void CreateSceneUniformBuffer() {
    wgpu::BufferDescriptor buffer_descriptor;
    buffer_descriptor.size = sizeof(SceneUniforms);
    buffer_descriptor.usage =
        wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    _scene_uniform_buffer = _device.CreateBuffer(&buffer_descriptor);
  }

  void CreateTextSampler() {
    wgpu::SamplerDescriptor sampler_descriptor;
    sampler_descriptor.addressModeU = wgpu::AddressMode::ClampToEdge;
    sampler_descriptor.addressModeV = wgpu::AddressMode::ClampToEdge;
    sampler_descriptor.addressModeW = wgpu::AddressMode::ClampToEdge;
    sampler_descriptor.magFilter = wgpu::FilterMode::Linear;
    sampler_descriptor.minFilter = wgpu::FilterMode::Linear;
    sampler_descriptor.mipmapFilter = wgpu::MipmapFilterMode::Nearest;
    _text_sampler = _device.CreateSampler(&sampler_descriptor);
  }

  void CreateTextTexture(uint32_t width, uint32_t height) {
    _text_texture_width = std::max<uint32_t>(1, width);
    _text_texture_height = std::max<uint32_t>(1, height);

    wgpu::TextureDescriptor texture_descriptor;
    texture_descriptor.usage =
        wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    texture_descriptor.dimension = wgpu::TextureDimension::e2D;
    texture_descriptor.size = {
        _text_texture_width,
        _text_texture_height,
        1,
    };
    texture_descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    _text_texture = _device.CreateTexture(&texture_descriptor);
    _text_texture_view = _text_texture.CreateView();
    CreateSceneBindGroup();
  }

  void UploadTextAtlas(const TextAtlas &atlas) {
    if (!_device) {
      return;
    }

    if (atlas.width != _text_texture_width ||
        atlas.height != _text_texture_height || !_text_texture) {
      CreateTextTexture(atlas.width, atlas.height);
    }
    if (!_text_texture || atlas.pixels.empty()) {
      return;
    }

    wgpu::TexelCopyTextureInfo destination;
    destination.texture = _text_texture;
    destination.aspect = wgpu::TextureAspect::All;

    wgpu::TexelCopyBufferLayout layout;
    layout.bytesPerRow = atlas.width * 4;
    layout.rowsPerImage = atlas.height;

    wgpu::Extent3D write_size{
        atlas.width,
        atlas.height,
        1,
    };
    _device.GetQueue().WriteTexture(&destination, atlas.pixels.data(),
                                    atlas.pixels.size(), &layout, &write_size);
  }

  void UpdateSceneUniforms() {
    if (!_scene_uniform_buffer) {
      return;
    }

    SceneLayout scene = LayoutScene(_root_view, _layout_size.width,
                                    _layout_size.height, _layout_context);
    TextAtlas text_atlas = BuildTextAtlas(scene.texts, scene.buttons, _scale);
    UploadTextAtlas(text_atlas);

    SceneUniforms uniforms = {};
    uniforms.viewport[0] = static_cast<float>(_width);
    uniforms.viewport[1] = static_cast<float>(_height);
    uniforms.viewport[3] = std::max(1.0f, _scale);
    uniforms.counts[0] = static_cast<float>(scene.panels.size());
    uniforms.counts[1] = static_cast<float>(scene.buttons.size());
    uniforms.counts[2] = static_cast<float>(text_atlas.entries.size());
    for (size_t index = 0; index < scene.panels.size(); ++index) {
      uniforms.panels[index] = MakePanel(scene.panels[index], _scale);
    }
    TextAtlasEntry empty_symbol;
    for (size_t index = 0; index < scene.buttons.size(); ++index) {
      const TextAtlasEntry &symbol =
          index < text_atlas.symbol_entries.size()
              ? text_atlas.symbol_entries[index]
              : empty_symbol;
      uniforms.buttons[index] = MakeSymbolButton(
          scene.buttons[index], symbol, _scale);
    }
    for (size_t index = 0; index < text_atlas.entries.size(); ++index) {
      uniforms.texts[index] = MakeText(text_atlas.entries[index], _scale);
    }
    _device.GetQueue().WriteBuffer(_scene_uniform_buffer, 0, &uniforms,
                                   sizeof(uniforms));
  }

  wgpu::Instance _instance;
  wgpu::Surface _surface;
  wgpu::Adapter _adapter;
  wgpu::Device _device;
  wgpu::Buffer _scene_uniform_buffer;
  wgpu::BindGroupLayout _bind_group_layout;
  wgpu::BindGroup _scene_bind_group;
  wgpu::Texture _text_texture;
  wgpu::TextureView _text_texture_view;
  wgpu::Sampler _text_sampler;
  wgpu::TextureFormat _format = wgpu::TextureFormat::Undefined;
  wgpu::RenderPipeline _pipeline;
  phenotype::ui::View _root_view;
  phenotype::ui::Size _layout_size;
  LayoutContext _layout_context;
  float _scale = 1.0f;
  uint32_t _width = 0;
  uint32_t _height = 0;
  uint32_t _text_texture_width = 0;
  uint32_t _text_texture_height = 0;
};

} // namespace

@protocol PhenotypeMetalViewDelegate
- (void)metalViewNeedsRender:(NSView *)view;
- (BOOL)metalView:(NSView *)view mouseDownAt:(NSPoint)location;
@end

@interface PhenotypeMetalView : NSView {
  id<PhenotypeMetalViewDelegate> _renderDelegate;
}
@property(nonatomic, assign) id<PhenotypeMetalViewDelegate> renderDelegate;
@end

@implementation PhenotypeMetalView

@synthesize renderDelegate = _renderDelegate;

- (void)requestRender {
  [_renderDelegate metalViewNeedsRender:self];
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
  (void)event;
  return YES;
}

- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

- (void)mouseDown:(NSEvent *)event {
  [[self window] makeFirstResponder:self];
  NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
  if (![_renderDelegate metalView:self mouseDownAt:location]) {
    [[self window] performWindowDragWithEvent:event];
  }
}

- (void)setFrameSize:(NSSize)newSize {
  NSSize old_size = [self frame].size;
  [super setFrameSize:newSize];
  if (!NSEqualSizes(old_size, newSize)) {
    [self requestRender];
  }
}

- (void)setBoundsSize:(NSSize)newSize {
  NSSize old_size = [self bounds].size;
  [super setBoundsSize:newSize];
  if (!NSEqualSizes(old_size, newSize)) {
    [self requestRender];
  }
}

- (void)viewWillStartLiveResize {
  [super viewWillStartLiveResize];
  [self requestRender];
}

- (void)viewDidEndLiveResize {
  [super viewDidEndLiveResize];
  [self requestRender];
}

@end

@interface AppDelegate
    : NSObject <NSApplicationDelegate, NSWindowDelegate,
                PhenotypeMetalViewDelegate> {
  NSWindow *_window;
  NSTimer *_render_timer;
  NSView *_metal_view;
  CAMetalLayer *_metal_layer;
  std::unique_ptr<DawnButtonRenderer> _renderer;
  phenotype::macos::window::Spec _spec;
  std::array<NSRect, 3> _window_control_base_frames;
  std::array<bool, 3> _window_control_has_base_frame;
  LayoutWindowControls _stable_window_controls;
  bool _has_window_control_base_frames;
  bool _is_rendering;
  bool _render_requested_after_current;
}
- (instancetype)initWithSpec:(phenotype::macos::window::Spec)spec;
- (LayoutContext)buildLayoutContext;
- (void)applyWindowControlOffset;
- (void)refreshRootView;
@end

@implementation AppDelegate

- (instancetype)initWithSpec:(phenotype::macos::window::Spec)spec {
  self = [super init];
  if (self) {
    _spec = std::move(spec);
    _window_control_base_frames.fill(NSZeroRect);
    _window_control_has_base_frame.fill(false);
    _stable_window_controls = {};
    _has_window_control_base_frames = false;
    _is_rendering = false;
    _render_requested_after_current = false;
  }
  return self;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  (void)notification;

  CGFloat window_width = std::max<CGFloat>(1.0, _spec.options.size.width);
  CGFloat window_height = std::max<CGFloat>(1.0, _spec.options.size.height);
  NSRect content_rect = NSMakeRect(0.0, 0.0, window_width, window_height);
  bool hides_title_bar = _spec.options.title_bar ==
                         phenotype::macos::window::TitleBarStyle::hidden;
  NSWindowStyleMask style_mask =
      NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
      NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
  if (hides_title_bar) {
    style_mask |= NSWindowStyleMaskFullSizeContentView;
  }

  _window = [[NSWindow alloc] initWithContentRect:content_rect
                                        styleMask:style_mask
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
  [_window setDelegate:self];
  [_window
      setTitle:[NSString stringWithUTF8String:_spec.options.title.c_str()]];
  ApplyTitleBarStyle(_window, _spec.options.title_bar);

  bool uses_blur = _spec.options.background.kind ==
                   phenotype::macos::window::Background::Kind::blurred;
  if (uses_blur) {
    [_window setOpaque:NO];
    [_window setBackgroundColor:[NSColor clearColor]];
  } else {
    [_window setOpaque:YES];
    [_window setBackgroundColor:[NSColor windowBackgroundColor]];
  }

  NSRect visible_frame = InitialWindowVisibleFrame(window_width, window_height);
  [_window setFrameOrigin:NSMakePoint(
                              NSMidX(visible_frame) - (window_width / 2.0),
                              NSMidY(visible_frame) - (window_height / 2.0))];

  NSView *content_view = nil;
  if (uses_blur) {
    NSVisualEffectView *visual_effect_view =
        [[NSVisualEffectView alloc] initWithFrame:content_rect];
    [visual_effect_view
        setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [visual_effect_view
        setMaterial:ToNativeVisualMaterial(
                        _spec.options.background.blur.material)];
    [visual_effect_view setBlendingMode:NSVisualEffectBlendingModeBehindWindow];
    [visual_effect_view setState:NSVisualEffectStateActive];
    [visual_effect_view setEmphasized:YES];
    [visual_effect_view setAlphaValue:_spec.options.background.blur.opacity];
    content_view = visual_effect_view;
  } else {
    content_view = [[NSView alloc] initWithFrame:content_rect];
    [content_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [content_view setWantsLayer:YES];
    [[content_view layer] setOpaque:NO];
    [[content_view layer] setBackgroundColor:[[NSColor clearColor] CGColor]];
  }

  _metal_view = [[PhenotypeMetalView alloc] initWithFrame:content_rect];
  [(PhenotypeMetalView *)_metal_view setRenderDelegate:self];
  [_metal_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [_metal_view setWantsLayer:YES];

  _metal_layer = [CAMetalLayer layer];
  [_metal_layer setOpaque:NO];
  [_metal_layer setBackgroundColor:[[NSColor clearColor] CGColor]];
  [_metal_layer setContentsGravity:kCAGravityTopLeft];
  [_metal_layer setFrame:NSRectToCGRect([_metal_view bounds])];
  [_metal_layer setContentsScale:[_window backingScaleFactor]];
  [_metal_layer
      setDrawableSize:CGSizeMake(LogicalPixel(window_width,
                                              [_window backingScaleFactor]),
                                 LogicalPixel(window_height,
                                              [_window backingScaleFactor]))];
  [_metal_view setLayer:_metal_layer];
  [content_view addSubview:_metal_view];
  [_metal_view release];

  [_window setContentView:content_view];
  [content_view release];
  ApplyTitleBarStyle(_window, _spec.options.title_bar);

  [_window makeKeyAndOrderFront:nil];
  [NSApp activate];
  [self applyWindowControlOffset];

  CGFloat scale = [_window backingScaleFactor];
  NSSize bounds = [_metal_view bounds].size;
  phenotype::ui::Size layout_size{
      static_cast<float>(bounds.width),
      static_cast<float>(bounds.height),
  };
  LayoutContext layout_context = [self buildLayoutContext];

  phenotype::ui::View root_view =
      _spec.content ? _spec.content() : phenotype::ui::empty();
  _renderer = std::make_unique<DawnButtonRenderer>();
  if (!_renderer->Initialize(_metal_layer, PixelSize(bounds.width, scale),
                             PixelSize(bounds.height, scale),
                             static_cast<float>(scale), layout_size,
                             layout_context, std::move(root_view))) {
    _renderer.reset();
    [NSApp terminate:nil];
    return;
  }

  _render_timer = [NSTimer timerWithTimeInterval:(1.0 / 60.0)
                                          target:self
                                        selector:@selector(renderFrame:)
                                        userInfo:nil
                                         repeats:YES];
  [[NSRunLoop mainRunLoop] addTimer:_render_timer forMode:NSRunLoopCommonModes];
  [self renderNow];
}

- (void)renderNow {
  if (!_window || !_metal_view || !_metal_layer) {
    return;
  }
  if (_is_rendering) {
    _render_requested_after_current = true;
    return;
  }
  _is_rendering = true;
  do {
    _render_requested_after_current = false;
    [self renderOnce];
  } while (_render_requested_after_current);
  _is_rendering = false;
}

- (void)refreshRootView {
  if (!_renderer || !_spec.content) {
    return;
  }
  _renderer->UpdateRootView(_spec.content());
}

- (void)renderOnce {
  CGFloat scale = [_window backingScaleFactor];
  NSRect bounds_rect = [_metal_view bounds];
  NSSize bounds = bounds_rect.size;
  if (bounds.width <= 0.0 || bounds.height <= 0.0) {
    return;
  }
  CGSize drawable_size =
      CGSizeMake(bounds.width * scale, bounds.height * scale);
  [_metal_layer setFrame:NSRectToCGRect(bounds_rect)];
  [_metal_layer setContentsScale:scale];
  [_metal_layer setDrawableSize:drawable_size];

  if (_renderer) {
    phenotype::ui::Size layout_size{
        static_cast<float>(bounds.width),
        static_cast<float>(bounds.height),
    };
    LayoutContext layout_context = [self buildLayoutContext];
    _renderer->Resize(PixelSize(bounds.width, scale),
                      PixelSize(bounds.height, scale),
                      static_cast<float>(scale), layout_size, layout_context);
    _renderer->Render();
  }
}

- (LayoutContext)buildLayoutContext {
  LayoutContext context;
  if (_stable_window_controls.has_leading_controls) {
    context.window_controls = _stable_window_controls;
    return context;
  }

  context = BuildLayoutContext(_window, [_window contentView]);
  if (context.window_controls.has_leading_controls) {
    _stable_window_controls = context.window_controls;
  }
  return context;
}

- (void)applyWindowControlOffset {
  if (!_has_window_control_base_frames) {
    _has_window_control_base_frames = CaptureWindowControlFrames(
        _window, _window_control_base_frames, _window_control_has_base_frame);
  }
  ApplyWindowControlVerticalOffset(
      _window, _window_control_base_frames, _window_control_has_base_frame,
      _spec.options.window_controls.vertical_offset);
}

- (void)windowDidResize:(NSNotification *)notification {
  if ([notification object] != _window) {
    return;
  }
  [self applyWindowControlOffset];
  [self renderNow];
}

- (void)windowDidEndLiveResize:(NSNotification *)notification {
  if ([notification object] != _window) {
    return;
  }
  [self applyWindowControlOffset];
  [self renderNow];
}

- (void)renderFrame:(NSTimer *)timer {
  (void)timer;
  [self renderNow];
}

- (void)metalViewNeedsRender:(NSView *)view {
  if (view == _metal_view) {
    [self renderNow];
  }
}

- (BOOL)metalView:(NSView *)view mouseDownAt:(NSPoint)location {
  if (view != _metal_view || !_renderer) {
    return NO;
  }
  NSSize bounds = [_metal_view bounds].size;
  phenotype::ui::Size layout_point{
      static_cast<float>(location.x),
      static_cast<float>(bounds.height - location.y),
  };
  if (_renderer->ActivateAt(layout_point)) {
    [self refreshRootView];
    [self renderNow];
    return YES;
  }
  return NO;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:
    (NSApplication *)sender {
  (void)sender;
  return YES;
}

- (void)dealloc {
  [_render_timer invalidate];
  [_window setDelegate:nil];
  [(PhenotypeMetalView *)_metal_view setRenderDelegate:nil];
  _renderer.reset();
  [_window release];
  [super dealloc];
}

@end

extern "C" int phenotype_macos_app_run(int argc, char *argv[],
                                       phenotype::macos::window::Spec *spec) {
  (void)argc;
  (void)argv;

  @autoreleasepool {
    NSApplication *application = [NSApplication sharedApplication];
    AppDelegate *delegate = [[AppDelegate alloc] initWithSpec:std::move(*spec)];

    [application setActivationPolicy:NSApplicationActivationPolicyRegular];
    [application setDelegate:delegate];
    [application run];

    [delegate release];
  }

  return 0;
}
