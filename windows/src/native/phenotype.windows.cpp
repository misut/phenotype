#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <windows.h>
#include <windowsx.h>

#include <phenotype/windows.hpp>

#pragma comment(lib, "gdi32.lib")

namespace {

namespace ui = phenotype::ui;

constexpr wchar_t kWindowClassName[] = L"PhenotypeWindow";
constexpr wchar_t kFallbackWindowTitle[] = L"Phenotype";
constexpr char kMaterialSymbolsFontFileName[] =
    "MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf";
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

struct WindowState {
  phenotype::windows::window::Spec *spec = nullptr;
  std::vector<HitTarget> hit_targets;
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
  return reinterpret_cast<WindowState *>(
      GetWindowLongPtrW(window, GWLP_USERDATA));
}

int RoundToInt(float value) {
  return static_cast<int>(std::lround(value));
}

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

bool HasWidth(ui::Size size) noexcept {
  return size.width > 0.0f;
}

bool HasHeight(ui::Size size) noexcept {
  return size.height > 0.0f;
}

int ColorComponent(float value, float alpha) {
  float const blended =
      std::clamp(value, 0.0f, 1.0f) * alpha + (1.0f - alpha);
  return std::clamp(static_cast<int>(std::lround(blended * 255.0f)), 0, 255);
}

COLORREF ToColorRef(ui::Color color) {
  float const alpha = std::clamp(color.alpha, 0.0f, 1.0f);
  return RGB(ColorComponent(color.red, alpha),
             ColorComponent(color.green, alpha),
             ColorComponent(color.blue, alpha));
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

  int const size = MultiByteToWideChar(CP_UTF8, 0, value.data(),
                                       static_cast<int>(value.size()), nullptr,
                                       0);
  if (size <= 0) {
    return {};
  }

  std::wstring result(static_cast<size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                      result.data(), size);
  return result;
}

std::filesystem::path SourceMaterialSymbolsFontPath() {
  std::filesystem::path source_file = __FILE__;
  return source_file.parent_path().parent_path().parent_path() / "resources" /
         "fonts" / kMaterialSymbolsFontFileName;
}

std::filesystem::path FindMaterialSymbolsFontPath() {
  std::vector<std::filesystem::path> candidates;

  std::error_code current_path_error;
  std::filesystem::path current_path =
      std::filesystem::current_path(current_path_error);
  if (!current_path_error) {
    candidates.push_back(current_path / "windows" / "resources" / "fonts" /
                         kMaterialSymbolsFontFileName);
    candidates.push_back(current_path / "resources" / "fonts" /
                         kMaterialSymbolsFontFileName);
    candidates.push_back(current_path / "../../../windows/resources/fonts" /
                         kMaterialSymbolsFontFileName);
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
      std::fprintf(stderr,
                   "phenotype: Material Symbols font file not found\n");
      return;
    }

    _font_count = AddFontResourceExW(_font_path.c_str(), FR_PRIVATE, nullptr);
    if (_font_count == 0) {
      std::fprintf(stderr,
                   "phenotype: failed to register Material Symbols font\n");
    }
  }

  MaterialSymbolsFontRegistration(const MaterialSymbolsFontRegistration &) =
      delete;
  MaterialSymbolsFontRegistration &
  operator=(const MaterialSymbolsFontRegistration &) = delete;

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

HFONT CreateFontForView(float font_size, float font_weight) {
  int const logical_height = -std::max(1, RoundToInt(font_size));
  int const weight =
      std::clamp(RoundToInt(font_weight), static_cast<int>(FW_THIN),
                 static_cast<int>(FW_HEAVY));
  return CreateFontW(logical_height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                     L"Segoe UI");
}

HFONT CreateMaterialSymbolsFont(ui::SymbolOptions options) {
  RegisterMaterialSymbolsFontIfAvailable();

  int const logical_height =
      -std::max(1, RoundToInt(options.optical_size));
  int const weight =
      std::clamp(RoundToInt(options.weight), static_cast<int>(FW_THIN),
                 static_cast<int>(FW_HEAVY));
  return CreateFontW(logical_height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                     DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
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
      .width = HasWidth(view.preferred_size)
                   ? view.preferred_size.width
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
  case ui::ViewKind::button_group: {
    ui::Size measured{};
    for (const ui::View &child : view.children) {
      ui::Size child_size = MeasureView(context, child);
      measured.width += child_size.width;
      measured.height = std::max(measured.height, child_size.height);
    }
    return measured;
  }
  case ui::ViewKind::stack: {
    ui::Size measured{};
    if (view.axis == ui::LayoutAxis::horizontal) {
      for (const ui::View &child : view.children) {
        ui::Size child_size = MeasureView(context, child);
        measured.width += child_size.width;
        measured.height = std::max(measured.height, child_size.height);
      }
      if (view.children.size() > 1) {
        measured.width += view.child_spacing *
                          static_cast<float>(view.children.size() - 1);
      }
    } else {
      for (const ui::View &child : view.children) {
        ui::Size child_size = MeasureView(context, child);
        measured.width = std::max(measured.width, child_size.width);
        measured.height += child_size.height;
      }
      if (view.children.size() > 1) {
        measured.height += view.child_spacing *
                           static_cast<float>(view.children.size() - 1);
      }
    }
    measured.width += view.content_padding.left + view.content_padding.right;
    measured.height += view.content_padding.top + view.content_padding.bottom;
    return measured;
  }
  default:
    return view.preferred_size;
  }
}

void AddHitTarget(WindowState &state, RectF rect, const ui::View &view,
                  bool is_enabled) {
  if (!view.click_action) {
    return;
  }

  state.hit_targets.push_back({
      .bounds = ToRect(rect),
      .is_enabled = is_enabled,
      .action = view.click_action,
  });
}

void FillRoundedRect(HDC context, RectF rect, float radius, ui::Color color) {
  RECT bounds = ToRect(rect);
  int const diameter = std::max(1, RoundToInt(radius * 2.0f));
  HBRUSH brush = CreateSolidBrush(ToColorRef(color));
  HPEN pen = static_cast<HPEN>(GetStockObject(NULL_PEN));
  ScopedSelect select_brush(context, brush);
  ScopedSelect select_pen(context, pen);
  RoundRect(context, bounds.left, bounds.top, bounds.right, bounds.bottom,
            diameter, diameter);
  DeleteObject(brush);
}

void StrokeLine(HDC context, float x1, float y1, float x2, float y2,
                ui::Color color) {
  HPEN pen = CreatePen(PS_SOLID, 2, ToColorRef(color));
  ScopedSelect select_pen(context, pen);
  MoveToEx(context, RoundToInt(x1), RoundToInt(y1), nullptr);
  LineTo(context, RoundToInt(x2), RoundToInt(y2));
  DeleteObject(pen);
}

void RenderView(HDC context, const ui::View &view, RectF rect,
                WindowState &state, bool inherited_enabled = true);

void RenderIcon(HDC context, const ui::View &view, RectF rect,
                bool is_enabled) {
  ui::Color color =
      is_enabled ? view.foreground_color : DisabledColor(view.foreground_color);
  wchar_t glyph[] = {MaterialSymbolCodepoint(view.symbol), L'\0'};
  HFONT font = CreateMaterialSymbolsFont(view.symbol_options);
  if (font == nullptr) {
    return;
  }
  RECT bounds = ToRect(rect);
  {
    ScopedSelect select_font(context, font);
    SetBkMode(context, TRANSPARENT);
    SetTextColor(context, ToColorRef(color));
    DrawTextW(context, glyph, 1, &bounds,
              DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
  }
  DeleteObject(font);
}

void RenderText(HDC context, const ui::View &view, RectF rect,
                bool is_enabled) {
  std::wstring const text = ToWide(view.text_content);
  HFONT font = CreateFontForView(view.font_size_value, view.font_weight_value);
  RECT bounds = ToRect(rect);
  {
    ScopedSelect select_font(context, font);
    SetBkMode(context, TRANSPARENT);
    SetTextColor(context, ToColorRef(is_enabled ? view.foreground_color
                                                : DisabledColor(
                                                      view.foreground_color)));
    DrawTextW(context, text.c_str(), static_cast<int>(text.size()), &bounds,
              DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
  }
  DeleteObject(font);
}

void RenderButton(HDC context, const ui::View &view, RectF rect,
                  WindowState &state, bool inherited_enabled) {
  bool const is_enabled = inherited_enabled && view.is_enabled;
  AddHitTarget(state, rect, view, is_enabled);

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

void RenderButtonGroup(HDC context, const ui::View &view, RectF rect,
                       WindowState &state, bool inherited_enabled) {
  FillRoundedRect(context, rect, rect.height * 0.5f,
                  {0.92f, 0.94f, 0.97f, 1.0f});

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
      StrokeLine(context, x, rect.y + 8.0f, x, rect.y + rect.height - 8.0f,
                 {0.78f, 0.81f, 0.86f, 1.0f});
    }
  }
}

void RenderStack(HDC context, const ui::View &view, RectF rect,
                 WindowState &state, bool inherited_enabled) {
  RectF content = Inset(rect, view.content_padding);
  float x = content.x;
  float y = content.y;

  for (const ui::View &child : view.children) {
    ui::Size child_size = MeasureView(context, child);
    if (view.axis == ui::LayoutAxis::horizontal) {
      RectF child_rect{
          .x = x,
          .y = content.y + (content.height - child_size.height) * 0.5f,
          .width = child_size.width,
          .height = child_size.height,
      };
      RenderView(context, child, child_rect, state, inherited_enabled);
      x += child_size.width + view.child_spacing;
    } else {
      RectF child_rect{
          .x = content.x,
          .y = y,
          .width = child_size.width,
          .height = child_size.height,
      };
      RenderView(context, child, child_rect, state, inherited_enabled);
      y += child_size.height + view.child_spacing;
    }
  }
}

void RenderView(HDC context, const ui::View &view, RectF rect,
                WindowState &state, bool inherited_enabled) {
  bool const is_enabled = inherited_enabled && view.is_enabled;
  switch (view.kind) {
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
  case ui::ViewKind::stack:
    RenderStack(context, view, rect, state, is_enabled);
    break;
  default:
    break;
  }
}

void FillSystemBackground(HDC context, RECT client) {
  HBRUSH brush = CreateSolidBrush(RGB(246, 248, 251));
  FillRect(context, &client, brush);
  DeleteObject(brush);
}

void PaintWindow(HWND window, HDC target, WindowState &state) {
  RECT client{};
  GetClientRect(window, &client);
  int const width =
      std::max(1, static_cast<int>(client.right - client.left));
  int const height =
      std::max(1, static_cast<int>(client.bottom - client.top));

  HDC buffer = CreateCompatibleDC(target);
  HBITMAP bitmap = CreateCompatibleBitmap(target, width, height);
  HGDIOBJ previous_bitmap = SelectObject(buffer, bitmap);

  FillSystemBackground(buffer, client);
  state.hit_targets.clear();

  if (state.spec != nullptr && state.spec->content) {
    ui::View content = state.spec->content();
    RenderView(buffer, content, FromRect(client), state);
  }

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

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam,
                            LPARAM lparam) {
  switch (message) {
  case WM_NCCREATE: {
    auto *create = reinterpret_cast<CREATESTRUCTW *>(lparam);
    SetWindowLongPtrW(window, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    return TRUE;
  }
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
  case WM_LBUTTONUP: {
    WindowState *state = GetWindowState(window);
    if (state == nullptr) {
      break;
    }

    POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    for (auto target = state->hit_targets.rbegin();
         target != state->hit_targets.rend(); ++target) {
      if (target->is_enabled && PtInRect(&target->bounds, point) &&
          target->action) {
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

HWND CreateMainWindow(HINSTANCE instance,
                      const phenotype::windows::window::Options &options,
                      WindowState *state) {
  DWORD const style = WS_OVERLAPPEDWINDOW;
  DWORD const extended_style = 0;
  int const width = std::max(1, static_cast<int>(options.size.width));
  int const height = std::max(1, static_cast<int>(options.size.height));
  RECT window_rect{0, 0, width, height};
  AdjustWindowRectEx(&window_rect, style, FALSE, extended_style);

  std::wstring title = ToWide(options.title);
  wchar_t const *title_text =
      title.empty() ? kFallbackWindowTitle : title.c_str();

  return CreateWindowExW(extended_style, kWindowClassName, title_text, style,
                         CW_USEDEFAULT, CW_USEDEFAULT,
                         window_rect.right - window_rect.left,
                         window_rect.bottom - window_rect.top, nullptr,
                         nullptr, instance, state);
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
    int argc, char *argv[], phenotype::windows::window::Spec *spec) {
  (void)argc;
  (void)argv;

  if (spec == nullptr) {
    return 1;
  }

  phenotype::windows::window::Spec window_spec = std::move(*spec);

  HINSTANCE const instance = GetModuleHandleW(nullptr);
  if (instance == nullptr || !RegisterMainWindowClass(instance)) {
    return 1;
  }

  WindowState window_state{&window_spec};
  HWND const window =
      CreateMainWindow(instance, window_spec.options, &window_state);
  if (window == nullptr) {
    return 1;
  }

  ShowWindow(window, InitialShowCommand());
  UpdateWindow(window);

  return RunMessageLoop();
}
