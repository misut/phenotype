#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include <windows.h>

#include <phenotype/windows.hpp>

namespace {

constexpr wchar_t kWindowClassName[] = L"PhenotypeWindow";
constexpr wchar_t kFallbackWindowTitle[] = L"Phenotype";

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
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  default:
    return DefWindowProcW(window, message, wparam, lparam);
  }
}

bool RegisterMainWindowClass(HINSTANCE instance) {
  WNDCLASSEXW window_class{};
  window_class.cbSize = sizeof(window_class);
  window_class.style = CS_HREDRAW | CS_VREDRAW;
  window_class.lpfnWndProc = WindowProc;
  window_class.hInstance = instance;
  window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  window_class.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
  window_class.lpszClassName = kWindowClassName;
  window_class.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);

  if (RegisterClassExW(&window_class) != 0) {
    return true;
  }
  return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

HWND CreateMainWindow(
    HINSTANCE instance,
    const phenotype::windows::window::Options &options) {
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
      nullptr);
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

  HWND const window = CreateMainWindow(instance, window_spec.options);
  if (window == nullptr) {
    return 1;
  }

  ShowWindow(window, InitialShowCommand());
  UpdateWindow(window);

  return RunMessageLoop();
}
