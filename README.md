# phenotype

C++ WASM UI framework using wasi-sdk + browser shim.

Write DOM-manipulating C++ code, compile to WebAssembly with wasi-sdk, and run in the browser.

## How it works

1. **C++ side**: Import the `phenotype` module. Use `create_element`, `set_text`, `append_child` to build DOM commands. Call `flush()` to execute them.
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

- **Command buffer**: C++ writes DOM commands to a fixed region of WASM linear memory, then calls `flush()`. The JS shim reads and executes all queued commands in one batch — minimizing WASM↔JS boundary crossings.
- **WASI polyfill**: Minimal `wasi_snapshot_preview1` implementation for browser (fd_write → console.log, proc_exit, etc.)
- **C++ modules**: Uses `.cppm` modules — supported by wasi-sdk's Clang.

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

## License

MIT
