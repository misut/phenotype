# phenotype

A cross-platform C++ UI framework. Currently targets the web via wasi-sdk + browser shim, with plans to support native platforms (Windows, macOS, Android, etc.) in the future.

## How it works

1. **C++ side**: Import the `phenotype` module. Use `create_element`, `set_text`, `append_child`, etc. to build DOM commands. Call `flush()` to execute them.
2. **Build**: `exon build --target wasm32-wasi` — produces a `.wasm` binary.
3. **Browser**: Load `phenotype.js` shim which provides WASI polyfill + DOM command buffer executor, then instantiates your `.wasm`.

```cpp
import phenotype;

int main() {
    auto h1 = phenotype::create_element("h1", 2);
    phenotype::set_text(h1, "Hello from C++!", 15);
    phenotype::append_child(0, h1); // 0 = document.body
    phenotype::flush();
    return 0;
}
```

```html
<script type="module">
    import { mount } from './phenotype.js';
    mount('hello.wasm');
</script>
```

## Architecture

- **Command buffer**: C++ writes DOM commands to a fixed region of WASM linear memory, then calls `flush()`. The JS shim reads and executes all queued commands in one batch — minimizing WASM-JS boundary crossings.
- **Platform-agnostic C++ API**: The C++ layer (`phenotype.cppm`) is runtime-independent. Platform-specific logic lives in the shim layer only, making it possible to add native backends in the future.
- **WASI polyfill**: Minimal `wasi_snapshot_preview1` implementation for browser (fd_write, proc_exit, etc.)
- **C++ modules**: Uses `.cppm` modules — supported by wasi-sdk's Clang.

## Available commands

| C++ function | DOM operation |
|---|---|
| `create_element(tag, len)` | `document.createElement(tag)` |
| `set_text(handle, text, len)` | `el.textContent = text` |
| `append_child(parent, child)` | `parent.appendChild(child)` |
| `set_attribute(handle, key, klen, val, vlen)` | `el.setAttribute(key, val)` |
| `set_style(handle, prop, plen, val, vlen)` | `el.style[prop] = val` |
| `set_inner_html(handle, html, len)` | `el.innerHTML = html` |
| `add_class(handle, cls, len)` | `el.classList.add(cls)` |
| `remove_child(parent, child)` | `parent.removeChild(child)` |
| `flush()` | Execute all queued commands |

## Requirements

- [exon](https://github.com/misut/exon) (C++ package manager)
- [intron](https://github.com/misut/intron) or wasi-sdk installed manually

## Quick start

```bash
cd examples/hello
exon build --target wasm32-wasi
cp .exon/wasm32-wasi/debug/hello hello.wasm
python3 -m http.server 8080
# Open http://localhost:8080/examples/hello/index.html
```

## Roadmap

- [x] Web (wasi-sdk + browser WASI shim)
- [ ] Windows (native backend)
- [ ] macOS (native backend)
- [ ] Android (native backend)

## License

MIT
