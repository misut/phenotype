#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>

#include <phenotype/layout.hpp>
#include <phenotype/scene.hpp>
#include <phenotype/windows.hpp>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "msimg32.lib")

namespace {

namespace ui = phenotype::ui;
namespace scene = phenotype::scene;
namespace layout = phenotype::layout;

constexpr wchar_t kWindowClassName[] = L"PhenotypeWindow";
constexpr wchar_t kFallbackWindowTitle[] = L"Phenotype";
constexpr float kTrafficLightLeft = 16.0f;
constexpr float kTrafficLightDiameter = 12.0f;
constexpr float kTrafficLightGap = 8.0f;
constexpr float kDefaultTopChromeMargin = 12.0f;
constexpr float kDefaultToolbarControlHeight = 36.0f;
constexpr float kDefaultTrafficLightCenterY =
    kDefaultTopChromeMargin + kDefaultToolbarControlHeight * 0.5f;
constexpr char kMaterialSymbolsFontFileName[] = "MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf";
constexpr wchar_t kMaterialSymbolsFontFamily[] = L"Material Symbols Rounded";

struct RectF {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

struct HitTarget {
  RECT bounds{};
  bool is_enabled = true;
  std::function<void()> action;
};

enum class CaptionButton {
  close,
  minimize,
  maximize_restore,
};

struct WindowState {
  phenotype::windows::window::Spec *spec = nullptr;
  phenotype::windows::window::TitleBarStyle title_bar =
      phenotype::windows::window::TitleBarStyle::visible;
  std::vector<HitTarget> hit_targets;
  bool uses_transparent_background = false;
  float traffic_light_center_y = kDefaultTrafficLightCenterY;
};

class ScopedSelect {
public:
  ScopedSelect(HDC context, HGDIOBJ object)
      : _context(context), _previous(SelectObject(context, object)) {}

  ScopedSelect(const ScopedSelect &) = delete;
  ScopedSelect &operator=(const ScopedSelect &) = delete;

  ~ScopedSelect() {
    if (_previous != nullptr) {
      SelectObject(_context, _previous);
    }
  }

private:
  HDC _context = nullptr;
  HGDIOBJ _previous = nullptr;
};

WindowState *GetWindowState(HWND window) noexcept {
  return reinterpret_cast<WindowState *>(GetWindowLongPtrW(window, GWLP_USERDATA));
}

int RoundToInt(float value) { return static_cast<int>(std::lround(value)); }

RECT ToRect(RectF rect) {
  int const left = RoundToInt(rect.x);
  int const top = RoundToInt(rect.y);
  int const right = std::max(left, RoundToInt(rect.x + rect.width));
  int const bottom = std::max(top, RoundToInt(rect.y + rect.height));
  return {left, top, right, bottom};
}

// Width occupied by the simulated traffic-light caption controls. The layout
// engine offsets leading content past this via LayoutContext::window_controls.
float LeadingCaptionControlsWidth() noexcept {
  return kTrafficLightLeft + kTrafficLightDiameter * 3.0f + kTrafficLightGap * 2.0f;
}

int ColorComponent(float value, float alpha) {
  float const blended = std::clamp(value, 0.0f, 1.0f) * alpha + (1.0f - alpha);
  return std::clamp(static_cast<int>(std::lround(blended * 255.0f)), 0, 255);
}

COLORREF ToColorRef(ui::Color color) {
  float const alpha = std::clamp(color.alpha, 0.0f, 1.0f);
  return RGB(ColorComponent(color.red, alpha), ColorComponent(color.green, alpha),
      ColorComponent(color.blue, alpha));
}

std::uint8_t ToByte(float value) {
  return static_cast<std::uint8_t>(
      std::clamp(static_cast<int>(std::lround(value * 255.0f)), 0, 255));
}

ui::Color DisabledColor(ui::Color color) {
  return {
      .red = color.red * 0.52f + 0.48f,
      .green = color.green * 0.52f + 0.48f,
      .blue = color.blue * 0.52f + 0.48f,
      .alpha = color.alpha,
  };
}

std::wstring ToWide(std::string_view value) {
  if (value.empty()) {
    return {};
  }

  int const size =
      MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
  if (size <= 0) {
    return {};
  }

  std::wstring result(static_cast<size_t>(size), L'\0');
  MultiByteToWideChar(
      CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
  return result;
}

std::filesystem::path SourceMaterialSymbolsFontPath() {
  std::filesystem::path source_file = __FILE__;
  return source_file.parent_path().parent_path().parent_path() / "resources" / "fonts" /
         kMaterialSymbolsFontFileName;
}

std::filesystem::path FindMaterialSymbolsFontPath() {
  std::vector<std::filesystem::path> candidates;

  std::error_code current_path_error;
  std::filesystem::path current_path = std::filesystem::current_path(current_path_error);
  if (!current_path_error) {
    candidates.push_back(
        current_path / "windows" / "resources" / "fonts" / kMaterialSymbolsFontFileName);
    candidates.push_back(current_path / "resources" / "fonts" / kMaterialSymbolsFontFileName);
    candidates.push_back(
        current_path / "../../../windows/resources/fonts" / kMaterialSymbolsFontFileName);
  }
  candidates.push_back(SourceMaterialSymbolsFontPath());

  for (const std::filesystem::path &candidate : candidates) {
    std::error_code error;
    if (std::filesystem::is_regular_file(candidate, error)) {
      return candidate;
    }
  }
  return {};
}

class MaterialSymbolsFontRegistration {
public:
  MaterialSymbolsFontRegistration() {
    _font_path = FindMaterialSymbolsFontPath();
    if (_font_path.empty()) {
      std::fprintf(stderr, "phenotype: Material Symbols font file not found\n");
      return;
    }

    _font_count = AddFontResourceExW(_font_path.c_str(), FR_PRIVATE, nullptr);
    if (_font_count == 0) {
      std::fprintf(stderr, "phenotype: failed to register Material Symbols font\n");
    }
  }

  MaterialSymbolsFontRegistration(const MaterialSymbolsFontRegistration &) = delete;
  MaterialSymbolsFontRegistration &operator=(const MaterialSymbolsFontRegistration &) = delete;

  ~MaterialSymbolsFontRegistration() {
    if (_font_count > 0) {
      RemoveFontResourceExW(_font_path.c_str(), FR_PRIVATE, nullptr);
    }
  }

  bool is_registered() const noexcept { return _font_count > 0; }

private:
  std::filesystem::path _font_path;
  int _font_count = 0;
};

bool RegisterMaterialSymbolsFontIfAvailable() {
  static MaterialSymbolsFontRegistration registration;
  return registration.is_registered();
}

bool WantsBlurredBackground(const phenotype::windows::window::Options &options) noexcept {
  return options.background.kind == phenotype::windows::window::Background::Kind::blurred;
}

bool ApplyBlurredBackground(HWND window) {
  // Windows SDK names: DWMWA_SYSTEMBACKDROP_TYPE = 38,
  // DWMSBT_TRANSIENTWINDOW = 3. Use values directly so older SDK headers still
  // compile while newer Windows 11 builds can enable Desktop Acrylic.
  constexpr DWORD kDwmSystemBackdropTypeAttribute = 38;
  constexpr int kDwmTransientWindowBackdrop = 3;

  int backdrop = kDwmTransientWindowBackdrop;
  HRESULT const backdrop_result =
      DwmSetWindowAttribute(window, kDwmSystemBackdropTypeAttribute, &backdrop, sizeof(backdrop));
  if (FAILED(backdrop_result)) {
    return false;
  }

  MARGINS margins{-1, -1, -1, -1};
  HRESULT const frame_result = DwmExtendFrameIntoClientArea(window, &margins);
  return SUCCEEDED(frame_result);
}

bool ApplyWindowBackground(HWND window, const phenotype::windows::window::Options &options) {
  if (!WantsBlurredBackground(options)) {
    return false;
  }
  return ApplyBlurredBackground(window);
}

bool WindowWantsHiddenTitleBar(HWND window) noexcept {
  WindowState *state = GetWindowState(window);
  return state != nullptr && state->title_bar == phenotype::windows::window::TitleBarStyle::hidden;
}

int ResizeBorderThickness() {
  return GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
}

bool IsPointInHitTarget(const WindowState &state, POINT point) noexcept {
  for (const HitTarget &target : state.hit_targets) {
    if (PtInRect(&target.bounds, point) != FALSE) {
      return true;
    }
  }
  return false;
}

float TrafficLightTop(float center_y) noexcept { return center_y - kTrafficLightDiameter * 0.5f; }

RectF CaptionButtonRect(CaptionButton button, float center_y) {
  constexpr float hit_padding_x = 4.0f;
  constexpr float hit_padding_y = 6.0f;
  int const index = [button] {
    switch (button) {
    case CaptionButton::close:
      return 0;
    case CaptionButton::minimize:
      return 1;
    case CaptionButton::maximize_restore:
      return 2;
    }
    return 0;
  }();
  float const x =
      kTrafficLightLeft + static_cast<float>(index) * (kTrafficLightDiameter + kTrafficLightGap);
  return {
      .x = x - hit_padding_x,
      .y = TrafficLightTop(center_y) - hit_padding_y,
      .width = kTrafficLightDiameter + hit_padding_x * 2.0f,
      .height = kTrafficLightDiameter + hit_padding_y * 2.0f,
  };
}

RectF CaptionButtonDotRect(CaptionButton button, float center_y) {
  int const index = [button] {
    switch (button) {
    case CaptionButton::close:
      return 0;
    case CaptionButton::minimize:
      return 1;
    case CaptionButton::maximize_restore:
      return 2;
    }
    return 0;
  }();
  return {
      .x = kTrafficLightLeft +
           static_cast<float>(index) * (kTrafficLightDiameter + kTrafficLightGap),
      .y = TrafficLightTop(center_y),
      .width = kTrafficLightDiameter,
      .height = kTrafficLightDiameter,
  };
}

bool IsPointInCaptionButtonArea(HWND window, POINT point) noexcept {
  WindowState *state = GetWindowState(window);
  float const center_y =
      state == nullptr ? kDefaultTrafficLightCenterY : state->traffic_light_center_y;
  for (CaptionButton button :
      {CaptionButton::close, CaptionButton::minimize, CaptionButton::maximize_restore}) {
    RECT bounds = ToRect(CaptionButtonRect(button, center_y));
    if (PtInRect(&bounds, point) != FALSE) {
      return true;
    }
  }
  return false;
}

LRESULT HitTestHiddenTitleBarWindow(HWND window, LPARAM lparam) {
  constexpr int kDragRegionHeight = 56;

  POINT screen_point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
  RECT window_rect{};
  GetWindowRect(window, &window_rect);

  POINT client_point = screen_point;
  ScreenToClient(window, &client_point);
  if (IsPointInCaptionButtonArea(window, client_point)) {
    return HTCLIENT;
  }

  int const border = ResizeBorderThickness();
  if (IsZoomed(window) == FALSE) {
    bool const is_left = screen_point.x < window_rect.left + border;
    bool const is_right = screen_point.x >= window_rect.right - border;
    bool const is_top = screen_point.y < window_rect.top + border;
    bool const is_bottom = screen_point.y >= window_rect.bottom - border;

    if (is_top && is_left) {
      return HTTOPLEFT;
    }
    if (is_top && is_right) {
      return HTTOPRIGHT;
    }
    if (is_bottom && is_left) {
      return HTBOTTOMLEFT;
    }
    if (is_bottom && is_right) {
      return HTBOTTOMRIGHT;
    }
    if (is_left) {
      return HTLEFT;
    }
    if (is_right) {
      return HTRIGHT;
    }
    if (is_top) {
      return HTTOP;
    }
    if (is_bottom) {
      return HTBOTTOM;
    }
  }

  WindowState *state = GetWindowState(window);
  if (state != nullptr && IsPointInHitTarget(*state, client_point)) {
    return HTCLIENT;
  }
  if (client_point.y < kDragRegionHeight) {
    return HTCAPTION;
  }
  return HTCLIENT;
}

HFONT CreateFontForView(float font_size, float font_weight) {
  int const logical_height = -std::max(1, RoundToInt(font_size));
  int const weight =
      std::clamp(RoundToInt(font_weight), static_cast<int>(FW_THIN), static_cast<int>(FW_HEAVY));
  return CreateFontW(logical_height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
      L"Segoe UI");
}

HFONT CreateMaterialSymbolsFont(ui::SymbolOptions options) {
  RegisterMaterialSymbolsFontIfAvailable();

  int const logical_height = -std::max(1, RoundToInt(options.optical_size));
  int const weight =
      std::clamp(RoundToInt(options.weight), static_cast<int>(FW_THIN), static_cast<int>(FW_HEAVY));
  return CreateFontW(logical_height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
      kMaterialSymbolsFontFamily);
}

wchar_t MaterialSymbolCodepoint(ui::Symbol symbol) noexcept {
  switch (symbol) {
  case ui::Symbol::chevron_left:
    return L'\xE5CB';
  case ui::Symbol::chevron_right:
    return L'\xE5CC';
  case ui::Symbol::folder:
    return L'\xE2C7';
  case ui::Symbol::description:
    return L'\xE873';
  }
  return L'\xE5CB';
}

// Adapts the GDI text metrics into the platform-neutral layout engine. The
// engine owns all sizing/flex/grid/scroll math (shared with macOS); GDI only
// answers "how wide is this text run". A scratch screen DC is used so the
// callback is self-contained and matches the DrawTextW path used at paint.
scene::MeasureTextFn MakeMeasureTextFn() {
  return [](std::string_view content, float font_size, float font_weight) -> ui::Size {
    if (content.empty()) {
      return {};
    }
    std::wstring const text = ToWide(content);
    HFONT font = CreateFontForView(font_size, font_weight);
    HDC screen = GetDC(nullptr);
    RECT bounds{0, 0, 10000, 10000};
    {
      ScopedSelect select_font(screen, font);
      DrawTextW(screen, text.c_str(), static_cast<int>(text.size()), &bounds,
          DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    }
    ReleaseDC(nullptr, screen);
    DeleteObject(font);
    return {
        .width = static_cast<float>(bounds.right - bounds.left),
        .height = std::max(static_cast<float>(bounds.bottom - bounds.top), font_size * 1.35f),
    };
  };
}

void AddActionHitTarget(WindowState &state, RectF rect, std::function<void()> action) {
  state.hit_targets.push_back({
      .bounds = ToRect(rect),
      .is_enabled = true,
      .action = std::move(action),
  });
}

constexpr int kRoundedRectSampleCountPerAxis = 4;
constexpr int kRoundedRectTotalSampleCount =
    kRoundedRectSampleCountPerAxis * kRoundedRectSampleCountPerAxis;

bool ContainsRoundedRectSample(float x, float y, float width, float height, float radius) {
  float const nearest_x = std::clamp(x, radius, width - radius);
  float const nearest_y = std::clamp(y, radius, height - radius);
  float const distance_x = x - nearest_x;
  float const distance_y = y - nearest_y;
  return distance_x * distance_x + distance_y * distance_y <= radius * radius;
}

std::uint8_t RoundedRectCoverage(int x, int y, int width, int height, float radius) {
  int covered_samples = 0;
  for (int sample_y = 0; sample_y < kRoundedRectSampleCountPerAxis; ++sample_y) {
    for (int sample_x = 0; sample_x < kRoundedRectSampleCountPerAxis; ++sample_x) {
      float const sample_local_x =
          static_cast<float>(x) + (static_cast<float>(sample_x) + 0.5f) /
                                      static_cast<float>(kRoundedRectSampleCountPerAxis);
      float const sample_local_y =
          static_cast<float>(y) + (static_cast<float>(sample_y) + 0.5f) /
                                      static_cast<float>(kRoundedRectSampleCountPerAxis);
      if (ContainsRoundedRectSample(sample_local_x, sample_local_y, static_cast<float>(width),
              static_cast<float>(height), radius)) {
        ++covered_samples;
      }
    }
  }

  return ToByte(
      static_cast<float>(covered_samples) / static_cast<float>(kRoundedRectTotalSampleCount));
}

std::uint32_t PremultipliedBgra(COLORREF color, std::uint8_t alpha) {
  std::uint8_t const red = static_cast<std::uint8_t>((GetRValue(color) * alpha + 127) / 255);
  std::uint8_t const green = static_cast<std::uint8_t>((GetGValue(color) * alpha + 127) / 255);
  std::uint8_t const blue = static_cast<std::uint8_t>((GetBValue(color) * alpha + 127) / 255);
  return (static_cast<std::uint32_t>(alpha) << 24) | (static_cast<std::uint32_t>(red) << 16) |
         (static_cast<std::uint32_t>(green) << 8) | static_cast<std::uint32_t>(blue);
}

std::uint32_t PremultipliedBgra(ui::Color color, std::uint8_t alpha) {
  std::uint8_t const red =
      static_cast<std::uint8_t>((ToByte(color.red) * static_cast<int>(alpha) + 127) / 255);
  std::uint8_t const green =
      static_cast<std::uint8_t>((ToByte(color.green) * static_cast<int>(alpha) + 127) / 255);
  std::uint8_t const blue =
      static_cast<std::uint8_t>((ToByte(color.blue) * static_cast<int>(alpha) + 127) / 255);
  return (static_cast<std::uint32_t>(alpha) << 24) | (static_cast<std::uint32_t>(red) << 16) |
         (static_cast<std::uint32_t>(green) << 8) | static_cast<std::uint32_t>(blue);
}

void FillRoundedRectMask(
    std::uint32_t *pixels, int width, int height, float radius, COLORREF color) {
  float const clamped_radius = std::clamp(
      radius, 0.0f, std::min(static_cast<float>(width), static_cast<float>(height)) * 0.5f);

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      pixels[y * width + x] =
          PremultipliedBgra(color, RoundedRectCoverage(x, y, width, height, clamped_radius));
    }
  }
}

bool FillAntialiasedRoundedRect(HDC context, RECT bounds, float radius, ui::Color color) {
  int const width = bounds.right - bounds.left;
  int const height = bounds.bottom - bounds.top;
  if (width <= 0 || height <= 0) {
    return true;
  }

  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = -height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;

  void *raw_pixels = nullptr;
  HBITMAP bitmap = CreateDIBSection(context, &info, DIB_RGB_COLORS, &raw_pixels, nullptr, 0);
  HDC source = CreateCompatibleDC(context);
  if (bitmap == nullptr || source == nullptr || raw_pixels == nullptr) {
    if (source != nullptr) {
      DeleteDC(source);
    }
    if (bitmap != nullptr) {
      DeleteObject(bitmap);
    }
    return false;
  }

  FillRoundedRectMask(
      static_cast<std::uint32_t *>(raw_pixels), width, height, radius, ToColorRef(color));

  HGDIOBJ previous_bitmap = SelectObject(source, bitmap);
  BLENDFUNCTION blend{
      .BlendOp = AC_SRC_OVER,
      .BlendFlags = 0,
      .SourceConstantAlpha = 255,
      .AlphaFormat = AC_SRC_ALPHA,
  };
  BOOL const blended = previous_bitmap != nullptr && previous_bitmap != HGDI_ERROR &&
                       AlphaBlend(context, bounds.left, bounds.top, width, height, source, 0, 0,
                           width, height, blend);
  if (previous_bitmap != nullptr && previous_bitmap != HGDI_ERROR) {
    SelectObject(source, previous_bitmap);
  }
  DeleteDC(source);
  DeleteObject(bitmap);
  return blended != FALSE;
}

void FillGdiRoundedRect(HDC context, RECT bounds, float radius, ui::Color color) {
  int const diameter = std::max(1, RoundToInt(radius * 2.0f));
  HBRUSH brush = CreateSolidBrush(ToColorRef(color));
  HPEN pen = static_cast<HPEN>(GetStockObject(NULL_PEN));
  ScopedSelect select_brush(context, brush);
  ScopedSelect select_pen(context, pen);
  RoundRect(context, bounds.left, bounds.top, bounds.right, bounds.bottom, diameter, diameter);
  DeleteObject(brush);
}

void FillRoundedRect(HDC context, RectF rect, float radius, ui::Color color) {
  RECT bounds = ToRect(rect);
  if (!FillAntialiasedRoundedRect(context, bounds, radius, color)) {
    FillGdiRoundedRect(context, bounds, radius, color);
  }
}

void FillButtonGroupSeparator(HDC context, float x, float y, float height) {
  constexpr ui::Color separator_color{0.72f, 0.76f, 0.82f, 1.0f};
  FillRoundedRect(context,
      {
          .x = x - 0.5f,
          .y = y,
          .width = 1.0f,
          .height = height,
      },
      0.5f, separator_color);
}

ui::Color CaptionButtonColor(CaptionButton button) noexcept {
  switch (button) {
  case CaptionButton::close:
    return {1.0f, 0.38f, 0.34f, 1.0f};
  case CaptionButton::minimize:
    return {1.0f, 0.76f, 0.20f, 1.0f};
  case CaptionButton::maximize_restore:
    return {0.22f, 0.78f, 0.35f, 1.0f};
  }
  return ui::white();
}

void DrawCaptionButton(HDC context, CaptionButton button, float center_y) {
  RectF dot = CaptionButtonDotRect(button, center_y);
  FillRoundedRect(context, dot, dot.width * 0.5f, CaptionButtonColor(button));
}

void AddCaptionButtonHitTargets(HWND window, WindowState &state, float center_y) {
  AddActionHitTarget(state, CaptionButtonRect(CaptionButton::close, center_y),
      [window] { PostMessageW(window, WM_CLOSE, 0, 0); });
  AddActionHitTarget(state, CaptionButtonRect(CaptionButton::minimize, center_y),
      [window] { ShowWindow(window, SW_MINIMIZE); });
  AddActionHitTarget(state, CaptionButtonRect(CaptionButton::maximize_restore, center_y),
      [window] { ShowWindow(window, IsZoomed(window) != FALSE ? SW_RESTORE : SW_MAXIMIZE); });
}

void RenderCaptionButtons(HWND window, HDC context, WindowState &state) {
  if (state.title_bar != phenotype::windows::window::TitleBarStyle::hidden) {
    return;
  }

  float const center_y = state.traffic_light_center_y;
  for (CaptionButton button :
      {CaptionButton::close, CaptionButton::minimize, CaptionButton::maximize_restore}) {
    DrawCaptionButton(context, button, center_y);
  }
  AddCaptionButtonHitTargets(window, state, center_y);
}

bool RenderGlyphMask(HDC context, ui::Symbol symbol, RectF rect, ui::Color color) {
  MAT2 matrix{
      {0, 1},
      {0, 0},
      {0, 0},
      {0, 1},
  };
  GLYPHMETRICS metrics{};
  wchar_t const glyph = MaterialSymbolCodepoint(symbol);
  DWORD const buffer_size = GetGlyphOutlineW(
      context, static_cast<UINT>(glyph), GGO_GRAY8_BITMAP, &metrics, 0, nullptr, &matrix);
  if (buffer_size == GDI_ERROR || buffer_size == 0 || metrics.gmBlackBoxX == 0 ||
      metrics.gmBlackBoxY == 0) {
    return false;
  }

  std::vector<std::uint8_t> mask(buffer_size);
  DWORD const result = GetGlyphOutlineW(context, static_cast<UINT>(glyph), GGO_GRAY8_BITMAP,
      &metrics, buffer_size, mask.data(), &matrix);
  if (result == GDI_ERROR) {
    return false;
  }

  int const width = static_cast<int>(metrics.gmBlackBoxX);
  int const height = static_cast<int>(metrics.gmBlackBoxY);
  int const row_stride = (width + 3) & ~3;
  size_t const required_mask_size = static_cast<size_t>(row_stride) * static_cast<size_t>(height);
  if (mask.size() < required_mask_size) {
    return false;
  }

  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = -height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;

  void *raw_pixels = nullptr;
  HBITMAP bitmap = CreateDIBSection(context, &info, DIB_RGB_COLORS, &raw_pixels, nullptr, 0);
  HDC source = CreateCompatibleDC(context);
  if (bitmap == nullptr || source == nullptr || raw_pixels == nullptr) {
    if (source != nullptr) {
      DeleteDC(source);
    }
    if (bitmap != nullptr) {
      DeleteObject(bitmap);
    }
    return false;
  }

  auto *pixels = static_cast<std::uint32_t *>(raw_pixels);
  std::uint8_t const color_alpha = ToByte(color.alpha);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      std::uint8_t const coverage = mask[static_cast<size_t>(y * row_stride + x)];
      std::uint8_t const alpha = static_cast<std::uint8_t>(
          (static_cast<int>(coverage) * static_cast<int>(color_alpha) + 32) / 64);
      pixels[y * width + x] = PremultipliedBgra(color, alpha);
    }
  }

  HGDIOBJ previous_bitmap = SelectObject(source, bitmap);
  BLENDFUNCTION blend{
      .BlendOp = AC_SRC_OVER,
      .BlendFlags = 0,
      .SourceConstantAlpha = 255,
      .AlphaFormat = AC_SRC_ALPHA,
  };
  int const draw_x = RoundToInt(rect.x + (rect.width - static_cast<float>(width)) * 0.5f);
  int const draw_y = RoundToInt(rect.y + (rect.height - static_cast<float>(height)) * 0.5f);
  BOOL const blended =
      previous_bitmap != nullptr && previous_bitmap != HGDI_ERROR &&
      AlphaBlend(context, draw_x, draw_y, width, height, source, 0, 0, width, height, blend);
  if (previous_bitmap != nullptr && previous_bitmap != HGDI_ERROR) {
    SelectObject(source, previous_bitmap);
  }
  DeleteDC(source);
  DeleteObject(bitmap);
  return blended != FALSE;
}

void RenderIconFallback(HDC context, ui::Symbol symbol, RectF rect, ui::Color color) {
  wchar_t glyph[] = {MaterialSymbolCodepoint(symbol), L'\0'};
  RECT bounds = ToRect(rect);
  SetBkMode(context, TRANSPARENT);
  SetTextColor(context, ToColorRef(color));
  DrawTextW(context, glyph, 1, &bounds, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
}

void RenderIcon(
    HDC context, ui::Symbol symbol, ui::SymbolOptions options, RectF rect, ui::Color color) {
  HFONT font = CreateMaterialSymbolsFont(options);
  if (font == nullptr) {
    return;
  }
  {
    ScopedSelect select_font(context, font);
    if (!RenderGlyphMask(context, symbol, rect, color)) {
      RenderIconFallback(context, symbol, rect, color);
    }
  }
  DeleteObject(font);
}

void RenderText(HDC context, const scene::TextLayout &text, RectF rect) {
  std::wstring const wide = ToWide(text.content);
  HFONT font = CreateFontForView(text.font_size, text.font_weight);
  RECT bounds = ToRect(rect);
  UINT format = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX;
  if (text.centers_text) {
    format |= DT_CENTER;
  }
  if (text.overflow == ui::TextOverflow::ellipsis) {
    if (text.truncation == ui::TextTruncation::middle) {
      format |= DT_PATH_ELLIPSIS;
    } else {
      format |= DT_END_ELLIPSIS;
    }
  }
  {
    ScopedSelect select_font(context, font);
    SetBkMode(context, TRANSPARENT);
    SetTextColor(context, ToColorRef(text.color));
    DrawTextW(context, wide.c_str(), static_cast<int>(wide.size()), &bounds, format);
  }
  DeleteObject(font);
}

RectF ToRectF(scene::LayoutRect rect) noexcept {
  return {.x = rect.x, .y = rect.y, .width = rect.width, .height = rect.height};
}

// Restricts GDI drawing to a clip rect for the duration of the scope. This is
// how the Windows backend honours the scene's clip rects (e.g. scroll
// viewports), a capability the old immediate-mode renderer lacked.
class ScopedClip {
public:
  ScopedClip(HDC context, std::optional<scene::LayoutRect> clip) : _context(context) {
    if (!clip) {
      return;
    }
    _region = CreateRectRgn(RoundToInt(clip->x), RoundToInt(clip->y),
        RoundToInt(clip->x + clip->width), RoundToInt(clip->y + clip->height));
    if (_region == nullptr) {
      return;
    }
    _previous = CreateRectRgn(0, 0, 0, 0);
    if (GetClipRgn(context, _previous) != 1) {
      DeleteObject(_previous);
      _previous = nullptr;
    }
    ExtSelectClipRgn(context, _region, RGN_AND);
    _active = true;
  }

  ScopedClip(const ScopedClip &) = delete;
  ScopedClip &operator=(const ScopedClip &) = delete;

  ~ScopedClip() {
    if (_active) {
      SelectClipRgn(_context, _previous);
    }
    if (_previous != nullptr) {
      DeleteObject(_previous);
    }
    if (_region != nullptr) {
      DeleteObject(_region);
    }
  }

private:
  HDC _context = nullptr;
  HRGN _region = nullptr;
  HRGN _previous = nullptr;
  bool _active = false;
};

void RenderScenePanel(HDC context, const scene::PanelLayout &panel) {
  ScopedClip clip(context, panel.clip_rect);
  FillRoundedRect(context, ToRectF(panel.frame), panel.corner_radius, panel.color);
}

// Paints a symbol button from the resolved scene record. The layout engine has
// already decided which button in a group draws the shared control background
// (draws_control) and the inter-button divider (draws_divider); GDI just obeys.
void RenderSceneButton(HDC context, const scene::SymbolButtonLayout &button) {
  ScopedClip clip(context, button.clip_rect);

  if (button.draws_control) {
    RectF control = ToRectF(button.control_frame);
    float const radius = scene::ControlShapeValue(button.control_shape) > 0.5f
                             ? control.height * 0.5f
                             : std::min(10.0f, std::min(control.width, control.height) * 0.5f);
    FillRoundedRect(context, control, radius, ui::white());
  }
  if (button.draws_divider) {
    RectF control = ToRectF(button.control_frame);
    FillButtonGroupSeparator(context, button.divider_x, control.y + 8.0f, control.height - 16.0f);
  }

  ui::Color const color = button.is_enabled ? button.color : DisabledColor(button.color);
  RenderIcon(context, button.symbol, button.options, ToRectF(button.frame), color);
}

void RenderSceneText(HDC context, const scene::TextLayout &text) {
  ScopedClip clip(context, text.clip_rect);
  RenderText(context, text, ToRectF(text.frame));
}

// Interprets one resolved draw layer in the same paint order the macOS shader
// composites: panels, then symbol buttons, then text runs.
void RenderSceneLayer(HDC context, const scene::SceneDrawLayer &layer) {
  for (const scene::PanelLayout &panel : layer.panels) {
    RenderScenePanel(context, panel);
  }
  for (const scene::SymbolButtonLayout &button : layer.buttons) {
    RenderSceneButton(context, button);
  }
  for (const scene::TextLayout &text : layer.texts) {
    RenderSceneText(context, text);
  }
}

// Paints a fully resolved scene. Effect panels have no GDI blur, so they fall
// back to a flat fill (matching the prior visual_effect_panel behaviour), then
// the background and foreground draw layers composite on top.
void RenderScene(HDC context, const scene::SceneLayout &scene_layout) {
  for (const scene::EffectPanelLayout &effect : scene_layout.effects) {
    ScopedClip clip(context, effect.clip_rect);
    FillRoundedRect(context, ToRectF(effect.frame), effect.corner_radius, effect.color);
  }
  RenderSceneLayer(context, scene_layout.background);
  RenderSceneLayer(context, scene_layout.foreground);
}

void FillWindowBackground(HDC context, RECT client, bool uses_transparent_background) {
  HBRUSH brush = CreateSolidBrush(uses_transparent_background ? RGB(0, 0, 0) : RGB(246, 248, 251));
  FillRect(context, &client, brush);
  DeleteObject(brush);
}

// Builds the layout context for a frame. When the title bar is hidden we draw
// our own traffic-light caption controls, so we advertise their bounds to the
// layout engine, which offsets leading toolbar content past them (the offset
// the old renderer applied by hand).
scene::LayoutContext BuildWindowsLayoutContext(const WindowState &state) {
  scene::LayoutContext context;
  if (state.title_bar != phenotype::windows::window::TitleBarStyle::hidden) {
    return context;
  }
  context.window_controls.has_leading_controls = true;
  context.window_controls.leading_controls = {
      .x = 0.0f,
      .y = TrafficLightTop(state.traffic_light_center_y),
      .width = LeadingCaptionControlsWidth(),
      .height = kTrafficLightDiameter,
  };
  return context;
}

void CollectHitTargets(WindowState &state, const scene::SceneLayout &scene_layout) {
  for (const scene::HitTargetLayout &target : scene_layout.hit_targets) {
    if (!target.action) {
      continue;
    }
    state.hit_targets.push_back({
        .bounds = ToRect(ToRectF(target.frame)),
        .is_enabled = target.is_enabled,
        .action = target.action,
    });
  }
}

void PaintWindow(HWND window, HDC target, WindowState &state) {
  RECT client{};
  GetClientRect(window, &client);
  int const width = std::max(1, static_cast<int>(client.right - client.left));
  int const height = std::max(1, static_cast<int>(client.bottom - client.top));

  HDC buffer = CreateCompatibleDC(target);
  HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
  HGDIOBJ previous_bitmap = SelectObject(buffer, bitmap);

  FillWindowBackground(buffer, client, state.uses_transparent_background);
  state.hit_targets.clear();

  if (state.spec != nullptr && state.spec->content) {
    ui::View content = state.spec->content();
    scene::LayoutContext context = BuildWindowsLayoutContext(state);
    scene::SceneLayout scene_layout = layout::LayoutScene(MakeMeasureTextFn(), content,
        static_cast<float>(width), static_cast<float>(height), context);
    RenderScene(buffer, scene_layout);
    CollectHitTargets(state, scene_layout);
  }
  RenderCaptionButtons(window, buffer, state);

  BitBlt(target, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
  SelectObject(buffer, previous_bitmap);
  DeleteObject(bitmap);
  DeleteDC(buffer);
}

int InitialShowCommand() noexcept {
  STARTUPINFOW startup_info{};
  startup_info.cb = sizeof(startup_info);
  GetStartupInfoW(&startup_info);
  if ((startup_info.dwFlags & STARTF_USESHOWWINDOW) != 0) {
    return startup_info.wShowWindow;
  }
  return SW_SHOWDEFAULT;
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
  case WM_NCCREATE: {
    auto *create = reinterpret_cast<CREATESTRUCTW *>(lparam);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    return TRUE;
  }
  case WM_NCCALCSIZE:
    if (wparam == TRUE && WindowWantsHiddenTitleBar(window)) {
      if (IsZoomed(window) != FALSE) {
        auto *parameters = reinterpret_cast<NCCALCSIZE_PARAMS *>(lparam);
        int const border = ResizeBorderThickness();
        parameters->rgrc[0].left += border;
        parameters->rgrc[0].top += border;
        parameters->rgrc[0].right -= border;
        parameters->rgrc[0].bottom -= border;
      }
      return 0;
    }
    break;
  case WM_NCHITTEST:
    if (WindowWantsHiddenTitleBar(window)) {
      return HitTestHiddenTitleBarWindow(window, lparam);
    }
    break;
  case WM_ERASEBKGND:
    return 1;
  case WM_PAINT: {
    WindowState *state = GetWindowState(window);
    PAINTSTRUCT paint{};
    HDC const context = BeginPaint(window, &paint);
    if (state != nullptr) {
      PaintWindow(window, context, *state);
    }
    EndPaint(window, &paint);
    return 0;
  }
  case WM_SIZE:
    InvalidateRect(window, nullptr, FALSE);
    return 0;
  case WM_DWMCOMPOSITIONCHANGED: {
    WindowState *state = GetWindowState(window);
    if (state != nullptr && state->spec != nullptr) {
      state->uses_transparent_background = ApplyWindowBackground(window, state->spec->options);
    }
    InvalidateRect(window, nullptr, FALSE);
    return 0;
  }
  case WM_LBUTTONUP: {
    WindowState *state = GetWindowState(window);
    if (state == nullptr) {
      break;
    }

    POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    for (auto target = state->hit_targets.rbegin(); target != state->hit_targets.rend(); ++target) {
      if (target->is_enabled && PtInRect(&target->bounds, point) && target->action) {
        target->action();
        InvalidateRect(window, nullptr, FALSE);
        return 0;
      }
    }
    break;
  }
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  case WM_NCDESTROY:
    SetWindowLongPtrW(window, GWLP_USERDATA, 0);
    break;
  default:
    break;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

bool RegisterMainWindowClass(HINSTANCE instance) {
  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.style = CS_HREDRAW | CS_VREDRAW;
  window_class.lpfnWndProc = WindowProc;
  window_class.hInstance = instance;
  window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.hbrBackground = nullptr;
  window_class.lpszClassName = kWindowClassName;
  window_class.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);

  if (RegisterClassExW(&window_class) != 0) {
    return true;
  }
  return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

DWORD WindowStyle(phenotype::windows::window::TitleBarStyle title_bar) {
  if (title_bar == phenotype::windows::window::TitleBarStyle::hidden) {
    return WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
  }
  return WS_OVERLAPPEDWINDOW;
}

HWND CreateMainWindow(HINSTANCE instance, const phenotype::windows::window::Options &options,
    phenotype::windows::window::TitleBarStyle title_bar, WindowState *state) {
  DWORD const style = WindowStyle(title_bar);
  DWORD const extended_style = 0;
  int const width = std::max(1, static_cast<int>(options.size.width));
  int const height = std::max(1, static_cast<int>(options.size.height));
  RECT window_rect{0, 0, width, height};
  if (title_bar != phenotype::windows::window::TitleBarStyle::hidden) {
    AdjustWindowRectEx(&window_rect, style, FALSE, extended_style);
  }

  std::wstring title = ToWide(options.title);
  wchar_t const *title_text = title.empty() ? kFallbackWindowTitle : title.c_str();

  HWND const window = CreateWindowExW(extended_style, kWindowClassName, title_text, style,
      CW_USEDEFAULT, CW_USEDEFAULT, window_rect.right - window_rect.left,
      window_rect.bottom - window_rect.top, nullptr, nullptr, instance, state);
  if (window != nullptr) {
    state->uses_transparent_background = ApplyWindowBackground(window, options);
  }
  return window;
}

int RunMessageLoop() {
  MSG message{};
  while (true) {
    BOOL const result = GetMessageW(&message, nullptr, 0, 0);
    if (result == -1) {
      return 1;
    }
    if (result == 0) {
      break;
    }

    TranslateMessage(&message);
    DispatchMessageW(&message);
  }

  return static_cast<int>(message.wParam);
}

} // namespace

extern "C" int phenotype_windows_app_run(
    int argc, char *argv[], phenotype::windows::window::Spec *spec, int title_bar) {
  (void)argc;
  (void)argv;

  if (spec == nullptr) {
    return 1;
  }

  phenotype::windows::window::Spec window_spec = std::move(*spec);
  phenotype::windows::window::TitleBarStyle title_bar_style =
      title_bar == static_cast<int>(phenotype::windows::window::TitleBarStyle::hidden)
          ? phenotype::windows::window::TitleBarStyle::hidden
          : phenotype::windows::window::TitleBarStyle::visible;

  HINSTANCE const instance = GetModuleHandleW(nullptr);
  if (instance == nullptr || !RegisterMainWindowClass(instance)) {
    return 1;
  }

  WindowState window_state;
  window_state.spec = &window_spec;
  window_state.title_bar = title_bar_style;
  HWND const window =
      CreateMainWindow(instance, window_spec.options, window_state.title_bar, &window_state);
  if (window == nullptr) {
    return 1;
  }

  ShowWindow(window, InitialShowCommand());
  UpdateWindow(window);

  return RunMessageLoop();
}
