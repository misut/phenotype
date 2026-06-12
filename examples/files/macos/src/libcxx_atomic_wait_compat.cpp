#include <cstdint>

// Dawn is built with the intron LLVM libc++ headers, while the app links the
// macOS system libc++ dylib. Forward LLVM 22 atomic-wait ABI symbols to the
// older system ABI so AppKit and Dawn share one libc++ runtime.
namespace std {
inline namespace __1 {

using __cxx_contention_t = long long;

__cxx_contention_t __libcpp_atomic_monitor(void const volatile*) noexcept;
void __libcpp_atomic_wait(void const volatile*, __cxx_contention_t) noexcept;
void __cxx_atomic_notify_one(void const volatile*) noexcept;
void __cxx_atomic_notify_all(void const volatile*) noexcept;

__cxx_contention_t __atomic_monitor_global(void const* address) noexcept {
    return __libcpp_atomic_monitor(address);
}

void __atomic_wait_global_table(void const* address, __cxx_contention_t monitor_value) noexcept {
    __libcpp_atomic_wait(address, monitor_value);
}

void __atomic_notify_one_global_table(void const* address) noexcept {
    __cxx_atomic_notify_one(address);
}

void __atomic_notify_all_global_table(void const* address) noexcept {
    __cxx_atomic_notify_all(address);
}

}
}
