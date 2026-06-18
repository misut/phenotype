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

#include <phenotype/windows.hpp>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "msimg32.lib")

namespace {

namespace ui = phenotype::ui;

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
  bool has_traffic_light_alignment = false;
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

RectF FromRect(RECT rect) {
  return {
      .x = static_cast<float>(rect.left),
      .y = static_cast<float>(rect.top),
      .width = static_cast<float>(rect.right - rect.left),
      .height = static_cast<float>(rect.bottom - rect.top),
  };
}

RectF Inset(RectF rect, ui::Insets insets) {
  rect.x += insets.left;
  rect.y += insets.top;
  rect.width = std::max(0.0f, rect.width - insets.left - insets.right);
  rect.height = std::max(0.0f, rect.height - insets.top - insets.bottom);
  return rect;
}

RectF Inset(RectF rect, float horizontal, float vertical) {
  return Inset(rect, {horizontal, vertical, horizontal, vertical});
}

bool HasWidth(ui::Size size) noexcept { return size.width > 0.0f; }

bool HasHeight(ui::Size size) noexcept { return size.height > 0.0f; }

float LeadingCaptionControlsWidth() noexcept {
  return kTrafficLightLeft + kTrafficLightDiameter * 3.0f + kTrafficLightGap * 2.0f;
}

float LeadingWindowControlsOffset(const ui::View &view) noexcept {
  if (!view.leading_window_controls_placement.is_enabled) {
    return 0.0f;
  }
  return LeadingCaptionControlsWidth() + view.leading_window_controls_placement.spacing;
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

ui::Size MeasureText(HDC context, const ui::View &view) {
  if (HasWidth(view.preferred_size) && HasHeight(view.preferred_size)) {
    return view.preferred_size;
  }

  std::wstring const text = ToWide(view.text_content);
  HFONT font = CreateFontForView(view.font_size_value, view.font_weight_value);
  RECT bounds{0, 0, 10000, 10000};
  {
    ScopedSelect select_font(context, font);
    DrawTextW(context, text.c_str(), static_cast<int>(text.size()), &bounds,
        DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
  }
  DeleteObject(font);

  return {
      .width = HasWidth(view.preferred_size) ? view.preferred_size.width
                                             : static_cast<float>(bounds.right - bounds.left),
      .height = HasHeight(view.preferred_size)
                    ? view.preferred_size.height
                    : std::max(static_cast<float>(bounds.bottom - bounds.top),
                          view.font_size_value * 1.35f),
  };
}

ui::Size MeasureView(HDC context, const ui::View &view) {
  switch (view.kind) {
  case ui::ViewKind::text:
    return MeasureText(context, view);
  case ui::ViewKind::button:
  case ui::ViewKind::icon:
    return view.preferred_size;
  case ui::ViewKind::grid:
    return {view.grid_min_column_width, view.grid_row_height};
  case ui::ViewKind::empty:
  case ui::ViewKind::spacer:
    return view.preferred_size;
  case ui::ViewKind::button_group:
  case ui::ViewKind::panel:
  case ui::ViewKind::stack:
    break;
  }

  ui::Size measured{};
  bool has_visible_child = false;
  for (const ui::View &child : view.children) {
    ui::Size child_size = MeasureView(context, child);
    if (view.axis == ui::LayoutAxis::horizontal) {
      if (has_visible_child) {
        measured.width += view.child_spacing;
      }
      measured.width += child_size.width;
      measured.height = std::max(measured.height, child_size.height);
    } else if (view.axis == ui::LayoutAxis::vertical) {
      if (has_visible_child) {
        measured.height += view.child_spacing;
      }
      measured.width = std::max(measured.width, child_size.width);
      measured.height += child_size.height;
    } else {
      measured.width = std::max(measured.width, child_size.width);
      measured.height = std::max(measured.height, child_size.height);
    }
    has_visible_child = true;
  }

  measured.width += view.content_padding.left + view.content_padding.right;
  measured.height += view.content_padding.top + view.content_padding.bottom;
  if (view.axis == ui::LayoutAxis::horizontal) {
    measured.width += LeadingWindowControlsOffset(view);
  }
  if (HasWidth(view.preferred_size)) {
    measured.width = view.preferred_size.width;
  }
  if (HasHeight(view.preferred_size)) {
    measured.height = view.preferred_size.height;
  }
  return measured;
}

void AddHitTarget(WindowState &state, RectF rect, const ui::View &view, bool is_enabled) {
  if (!view.click_action) {
    return;
  }

  state.hit_targets.push_back({
      .bounds = ToRect(rect),
      .is_enabled = is_enabled,
      .action = view.click_action,
  });
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

void RenderView(HDC context, const ui::View &view, RectF rect, WindowState &state,
    bool inherited_enabled = true);

bool RenderGlyphMask(HDC context, const ui::View &view, RectF rect, ui::Color color) {
  MAT2 matrix{
      {0, 1},
      {0, 0},
      {0, 0},
      {0, 1},
  };
  GLYPHMETRICS metrics{};
  wchar_t const glyph = MaterialSymbolCodepoint(view.symbol);
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

void RenderIconFallback(HDC context, const ui::View &view, RectF rect, ui::Color color) {
  wchar_t glyph[] = {MaterialSymbolCodepoint(view.symbol), L'\0'};
  RECT bounds = ToRect(rect);
  SetBkMode(context, TRANSPARENT);
  SetTextColor(context, ToColorRef(color));
  DrawTextW(context, glyph, 1, &bounds, DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
}

void RenderIcon(HDC context, const ui::View &view, RectF rect, bool is_enabled) {
  ui::Color color = is_enabled ? view.foreground_color : DisabledColor(view.foreground_color);
  HFONT font = CreateMaterialSymbolsFont(view.symbol_options);
  if (font == nullptr) {
    return;
  }
  {
    ScopedSelect select_font(context, font);
    if (!RenderGlyphMask(context, view, rect, color)) {
      RenderIconFallback(context, view, rect, color);
    }
  }
  DeleteObject(font);
}

void RenderText(HDC context, const ui::View &view, RectF rect, bool is_enabled) {
  std::wstring const text = ToWide(view.text_content);
  HFONT font = CreateFontForView(view.font_size_value, view.font_weight_value);
  RECT bounds = ToRect(rect);
  UINT format = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX;
  if (view.centers_text) {
    format |= DT_CENTER;
  }
  if (view.text_overflow == ui::TextOverflow::ellipsis) {
    if (view.text_truncation == ui::TextTruncation::middle) {
      format |= DT_PATH_ELLIPSIS;
    } else {
      format |= DT_END_ELLIPSIS;
    }
  }
  {
    ScopedSelect select_font(context, font);
    SetBkMode(context, TRANSPARENT);
    SetTextColor(context,
        ToColorRef(is_enabled ? view.foreground_color : DisabledColor(view.foreground_color)));
    DrawTextW(context, text.c_str(), static_cast<int>(text.size()), &bounds, format);
  }
  DeleteObject(font);
}

void RenderButton(
    HDC context, const ui::View &view, RectF rect, WindowState &state, bool inherited_enabled) {
  bool const is_enabled = inherited_enabled && view.is_enabled;

  for (const ui::View &child : view.children) {
    ui::Size child_size = MeasureView(context, child);
    RectF child_rect{
        .x = rect.x + (rect.width - child_size.width) * 0.5f,
        .y = rect.y + (rect.height - child_size.height) * 0.5f,
        .width = child_size.width,
        .height = child_size.height,
    };
    RenderView(context, child, child_rect, state, is_enabled);
  }
}

void RenderPanel(HDC context, const ui::View &view, RectF rect) {
  FillRoundedRect(context, rect, view.corner_radius_value, view.background_color);
}

void RenderGrid(
    HDC context, const ui::View &view, RectF rect, WindowState &state, bool inherited_enabled) {
  RectF content = Inset(rect, view.content_padding);
  float const min_column_width = std::max(1.0f, view.grid_min_column_width);
  float const column_gap = std::max(0.0f, view.grid_column_gap);
  float const row_gap = std::max(0.0f, view.grid_row_gap);
  std::size_t const column_count = std::max<std::size_t>(
      1, static_cast<std::size_t>((content.width + column_gap) / (min_column_width + column_gap)));
  float const total_gap = column_gap * static_cast<float>(column_count - 1);
  float const cell_width =
      std::max(1.0f, (content.width - total_gap) / static_cast<float>(column_count));
  float const row_height = std::max(1.0f, view.grid_row_height);

  for (std::size_t index = 0; index < view.children.size(); ++index) {
    std::size_t const row = index / column_count;
    std::size_t const column = index % column_count;
    RectF child_rect{
        .x = content.x + static_cast<float>(column) * (cell_width + column_gap),
        .y = content.y + static_cast<float>(row) * (row_height + row_gap),
        .width = cell_width,
        .height = row_height,
    };
    RenderView(context, view.children[index], child_rect, state, inherited_enabled);
  }
}

void RenderButtonGroup(
    HDC context, const ui::View &view, RectF rect, WindowState &state, bool inherited_enabled) {
  FillRoundedRect(context, rect, rect.height * 0.5f, ui::white());

  float x = rect.x;
  for (std::size_t index = 0; index < view.children.size(); ++index) {
    const ui::View &child = view.children[index];
    ui::Size child_size = MeasureView(context, child);
    RectF child_rect{
        .x = x,
        .y = rect.y,
        .width = child_size.width,
        .height = rect.height,
    };
    RenderView(context, child, child_rect, state, inherited_enabled);
    x += child_size.width;

    if (index + 1 < view.children.size()) {
      FillButtonGroupSeparator(context, x, rect.y + 8.0f, rect.height - 16.0f);
    }
  }
}

void RenderStack(
    HDC context, const ui::View &view, RectF rect, WindowState &state, bool inherited_enabled) {
  RectF content = Inset(rect, view.content_padding);
  if (view.axis == ui::LayoutAxis::horizontal &&
      view.leading_window_controls_placement.is_enabled &&
      view.leading_window_controls_placement.aligns_vertical_center &&
      !state.has_traffic_light_alignment) {
    state.traffic_light_center_y = content.y + content.height * 0.5f;
    state.has_traffic_light_alignment = true;
  }
  if (view.axis == ui::LayoutAxis::horizontal &&
      state.title_bar == phenotype::windows::window::TitleBarStyle::hidden) {
    float const offset = LeadingWindowControlsOffset(view);
    content.x += offset;
    content.width = std::max(0.0f, content.width - offset);
  }

  if (view.axis == ui::LayoutAxis::overlay) {
    for (const ui::View &child : view.children) {
      RenderView(context, child, content, state, inherited_enabled);
    }
    return;
  }

  float const available_main =
      view.axis == ui::LayoutAxis::horizontal ? content.width : content.height;
  std::size_t flexible_child_count = 0;
  float fixed_main = view.children.empty()
                         ? 0.0f
                         : view.child_spacing * static_cast<float>(view.children.size() - 1);
  for (const ui::View &child : view.children) {
    ui::Size child_size = MeasureView(context, child);
    bool const expands_on_axis =
        view.axis == ui::LayoutAxis::horizontal ? child.expands_width : child.expands_height;
    if (expands_on_axis) {
      ++flexible_child_count;
    } else {
      fixed_main += view.axis == ui::LayoutAxis::horizontal ? child_size.width : child_size.height;
    }
  }

  float flexible_main = 0.0f;
  if (flexible_child_count > 0) {
    flexible_main =
        std::max(0.0f, available_main - fixed_main) / static_cast<float>(flexible_child_count);
  }

  float x = content.x;
  float y = content.y;
  for (const ui::View &child : view.children) {
    ui::Size child_size = MeasureView(context, child);
    if (view.axis == ui::LayoutAxis::horizontal) {
      if (child.expands_width) {
        child_size.width = flexible_main;
      }
      if (child.expands_height) {
        child_size.height = content.height;
      }
      RectF child_rect{
          .x = x,
          .y = child.expands_height
                   ? content.y
                   : content.y + std::max(0.0f, content.height - child_size.height) * 0.5f,
          .width = child_size.width,
          .height = child_size.height,
      };
      RenderView(context, child, child_rect, state, inherited_enabled);
      x += child_size.width + view.child_spacing;
    } else {
      if (child.expands_width) {
        child_size.width = content.width;
      }
      if (child.expands_height) {
        child_size.height = flexible_main;
      }
      float child_x = content.x;
      if (view.centers_children && !child.expands_width) {
        child_x = content.x + std::max(0.0f, content.width - child_size.width) * 0.5f;
      }
      RectF child_rect{
          .x = child_x,
          .y = y,
          .width = child_size.width,
          .height = child_size.height,
      };
      RenderView(context, child, child_rect, state, inherited_enabled);
      y += child_size.height + view.child_spacing;
    }
  }
}

void RenderView(
    HDC context, const ui::View &view, RectF rect, WindowState &state, bool inherited_enabled) {
  bool const is_enabled = inherited_enabled && view.is_enabled;
  AddHitTarget(state, rect, view, is_enabled);

  switch (view.kind) {
  case ui::ViewKind::empty:
  case ui::ViewKind::spacer:
    break;
  case ui::ViewKind::button:
    RenderButton(context, view, rect, state, inherited_enabled);
    break;
  case ui::ViewKind::button_group:
    RenderButtonGroup(context, view, rect, state, is_enabled);
    break;
  case ui::ViewKind::icon:
    RenderIcon(context, view, rect, is_enabled);
    break;
  case ui::ViewKind::text:
    RenderText(context, view, rect, is_enabled);
    break;
  case ui::ViewKind::panel:
    RenderPanel(context, view, rect);
    break;
  case ui::ViewKind::grid:
    RenderGrid(context, view, rect, state, is_enabled);
    break;
  case ui::ViewKind::stack:
    RenderStack(context, view, rect, state, is_enabled);
    break;
  }
}

void FillWindowBackground(HDC context, RECT client, bool uses_transparent_background) {
  HBRUSH brush = CreateSolidBrush(uses_transparent_background ? RGB(0, 0, 0) : RGB(246, 248, 251));
  FillRect(context, &client, brush);
  DeleteObject(brush);
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
  state.traffic_light_center_y = kDefaultTrafficLightCenterY;
  state.has_traffic_light_alignment = false;

  if (state.spec != nullptr && state.spec->content) {
    ui::View content = state.spec->content();
    RenderView(buffer, content, FromRect(client), state);
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
