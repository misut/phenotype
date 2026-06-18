#include <cstddef>
#include <cstdint>

// Dawn is built with the intron LLVM libc++ headers, while the app links the
// macOS system libc++ dylib. Forward LLVM 22 libc++ ABI symbols to the older
// system ABI so AppKit and Dawn share one libc++ runtime.
namespace std {
inline namespace __1 {

using __cxx_contention_t = long long;

__cxx_contention_t __libcpp_atomic_monitor(void const volatile *) noexcept;
void __libcpp_atomic_wait(void const volatile *, __cxx_contention_t) noexcept;
void __cxx_atomic_notify_one(void const volatile *) noexcept;
void __cxx_atomic_notify_all(void const volatile *) noexcept;

__cxx_contention_t __atomic_monitor_global(void const *address) noexcept {
  return __libcpp_atomic_monitor(address);
}

void __atomic_wait_global_table(void const *address, __cxx_contention_t monitor_value) noexcept {
  __libcpp_atomic_wait(address, monitor_value);
}

void __atomic_notify_one_global_table(void const *address) noexcept {
  __cxx_atomic_notify_one(address);
}

void __atomic_notify_all_global_table(void const *address) noexcept {
  __cxx_atomic_notify_all(address);
}

std::size_t __hash_memory(void const *data, std::size_t length) noexcept {
  auto const *bytes = static_cast<std::uint8_t const *>(data);
  std::uint64_t hash = 14695981039346656037ull;

  for (std::size_t index = 0; index < length; ++index) {
    hash ^= bytes[index];
    hash *= 1099511628211ull;
  }

  return static_cast<std::size_t>(hash);
}

} // namespace __1
} // namespace std
