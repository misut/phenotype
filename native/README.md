# phenotype native (Dawn/WebGPU)

Native rendering prototype using Google Dawn (WebGPU C++ implementation).
Same UI framework, same WGSL shaders — runs as a native macOS/Windows/Linux
window instead of a browser tab.

## Prerequisites

- CMake 3.30+
- Clang 22+ (via `intron install`)
- Git (for Dawn clone)

## Setup

```bash
# One-time: clone Dawn (shallow, ~200 MB)
git clone https://dawn.googlesource.com/dawn third_party/dawn --depth=1

# Build (first time ~15 min — Dawn compiles its deps)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(sysctl -n hw.logicalcpu)

# Run
./build/phenotype-native
```

## How it works

The native runner implements the 5 host functions from `phenotype_host.h`:

| Function | Native implementation |
|---|---|
| `phenotype_flush()` | Parses cmd buffer → Dawn WebGPU render pass |
| `phenotype_measure_text()` | stb_truetype font metrics (Phase C; stub in Phase A) |
| `phenotype_get_canvas_width/height()` | GLFW window size |
| `phenotype_open_url()` | System shell (Phase D) |

The phenotype library core is unchanged — same widgets, layout, diff, paint.
Only the backend differs (Dawn instead of JS shim).

## Architecture

See [docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md) for the full rendering
pipeline, host interface contract, and Dawn/WebGPU strategy.
