#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include <windows.h>
#include <dwmapi.h>

#include <phenotype/windows.hpp>

namespace {

constexpr wchar_t kWindowClassName[] = L"PhenotypeWindow";
constexpr wchar_t kFallbackWindowTitle[] = L"Phenotype";
constexpr DWORD kDwmWindowAttributeSystemBackdropType = 38;
constexpr DWORD kDwmWindowAttributeRedirectionBitmapAlpha = 39;
constexpr DWORD kDwmSystemBackdropNone = 1;
constexpr DWORD kDwmSystemBackdropMainWindow = 2;
constexpr DWORD kDwmSystemBackdropTransientWindow = 3;
constexpr DWORD kDwmSystemBackdropTabbedWindow = 4;

struct WindowState {
  const phenotype::windows::window::Options *options = nullptr;
};

WindowState *GetWindowState(HWND window) noexcept {
  return reinterpret_cast<WindowState *>(
      GetWindowLongPtrW(window, GWLP_USERDATA));
}

bool UsesBlurredBackground(
    const phenotype::windows::window::Options &options) noexcept {
  return options.background.kind ==
         phenotype::windows::window::Background::Kind::blurred;
}

DWORD ToNativeSystemBackdrop(
    phenotype::windows::window::VisualMaterial material) noexcept {
  switch (material) {
  case phenotype::windows::window::VisualMaterial::mica:
    return kDwmSystemBackdropMainWindow;
  case phenotype::windows::window::VisualMaterial::mica_alt:
    return kDwmSystemBackdropTabbedWindow;
  case phenotype::windows::window::VisualMaterial::desktop_acrylic:
    return kDwmSystemBackdropTransientWindow;
  }
  return kDwmSystemBackdropTransientWindow;
}

void EnableBlurBehind(HWND window) {
  DWM_BLURBEHIND blur{};
  blur.dwFlags = DWM_BB_ENABLE;
  blur.fEnable = TRUE;
  DwmEnableBlurBehindWindow(window, &blur);
}

void ExtendFrameIntoClientArea(HWND window, bool enabled) {
  MARGINS margins =
      enabled ? MARGINS{-1, -1, -1, -1} : MARGINS{0, 0, 0, 0};
  DwmExtendFrameIntoClientArea(window, &margins);
}

bool SetRedirectionBitmapAlpha(HWND window, bool enabled) {
  BOOL value = enabled ? TRUE : FALSE;
  return SUCCEEDED(DwmSetWindowAttribute(
      window, kDwmWindowAttributeRedirectionBitmapAlpha, &value,
      sizeof(value)));
}

void ApplyWindowBackground(
    HWND window, const phenotype::windows::window::Options &options) {
  if (!UsesBlurredBackground(options)) {
    ExtendFrameIntoClientArea(window, false);
    SetRedirectionBitmapAlpha(window, false);

    DWORD backdrop = kDwmSystemBackdropNone;
    DwmSetWindowAttribute(window, kDwmWindowAttributeSystemBackdropType,
                          &backdrop, sizeof(backdrop));
    InvalidateRect(window, nullptr, TRUE);
    return;
  }

  ExtendFrameIntoClientArea(window, true);
  bool const uses_redirection_alpha = SetRedirectionBitmapAlpha(window, true);

  DWORD backdrop =
      ToNativeSystemBackdrop(options.background.blur.material);
  HRESULT const result = DwmSetWindowAttribute(
      window, kDwmWindowAttributeSystemBackdropType, &backdrop,
      sizeof(backdrop));
  if (FAILED(result) || !uses_redirection_alpha) {
    EnableBlurBehind(window);
  }
  InvalidateRect(window, nullptr, TRUE);
}

void FillSystemBackground(HWND window, HDC context) {
  RECT client{};
  GetClientRect(window, &client);
  FillRect(context, &client, GetSysColorBrush(COLOR_WINDOW));
}

void ClearTransparentBackground(HWND window, HDC context) {
  RECT client{};
  GetClientRect(window, &client);
  FillRect(context, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
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
  case WM_ERASEBKGND: {
    WindowState const *state = GetWindowState(window);
    if (state != nullptr && state->options != nullptr &&
        UsesBlurredBackground(*state->options)) {
      ClearTransparentBackground(window, reinterpret_cast<HDC>(wparam));
      return 1;
    }
    FillSystemBackground(window, reinterpret_cast<HDC>(wparam));
    return 1;
  }
  case WM_PAINT: {
    WindowState const *state = GetWindowState(window);
    PAINTSTRUCT paint{};
    HDC const context = BeginPaint(window, &paint);
    if (state != nullptr && state->options != nullptr &&
        UsesBlurredBackground(*state->options)) {
      ClearTransparentBackground(window, context);
    } else {
      FillSystemBackground(window, context);
    }
    EndPaint(window, &paint);
    return 0;
  }
  case WM_DWMCOMPOSITIONCHANGED: {
    WindowState const *state = GetWindowState(window);
    if (state != nullptr && state->options != nullptr) {
      ApplyWindowBackground(window, *state->options);
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

HWND CreateMainWindow(
    HINSTANCE instance,
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

  return CreateWindowExW(
      extended_style, kWindowClassName, title_text, style, CW_USEDEFAULT,
      CW_USEDEFAULT, window_rect.right - window_rect.left,
      window_rect.bottom - window_rect.top, nullptr, nullptr, instance,
      state);
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
  if (window_spec.content) {
    (void)window_spec.content();
  }

  HINSTANCE const instance = GetModuleHandleW(nullptr);
  if (instance == nullptr || !RegisterMainWindowClass(instance)) {
    return 1;
  }

  WindowState window_state{&window_spec.options};
  HWND const window =
      CreateMainWindow(instance, window_spec.options, &window_state);
  if (window == nullptr) {
    return 1;
  }

  ApplyWindowBackground(window, window_spec.options);

  ShowWindow(window, InitialShowCommand());
  UpdateWindow(window);

  return RunMessageLoop();
}
