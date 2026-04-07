// phenotype.js — WASI polyfill + DOM command buffer executor
//
// Usage:
//   import { mount } from './phenotype.js';
//   mount('hello.wasm', document.body);

// --- DOM command buffer executor ---

const CMD_CREATE_ELEMENT = 1;
const CMD_SET_TEXT = 2;
const CMD_APPEND_CHILD = 3;

function createExecutor(instance, rootElement) {
  const handles = new Map();
  handles.set(0, rootElement); // handle 0 = root element

  function readString(bytes, offset, len) {
    return new TextDecoder().decode(bytes.slice(offset, offset + len));
  }

  return function flush() {
    const mem = instance.exports.memory.buffer;

    // Get command buffer location via exported accessor functions
    const bufOffset = instance.exports.phenotype_get_cmd_buf();
    const bufLen = instance.exports.phenotype_get_cmd_len();

    if (bufLen === 0) return;

    const bytes = new Uint8Array(mem);
    let pos = bufOffset;
    const end = bufOffset + bufLen;

    while (pos < end) {
      const cmd = view.getUint32(pos, true); pos += 4;

      switch (cmd) {
        case CMD_CREATE_ELEMENT: {
          const handle = view.getUint32(pos, true); pos += 4;
          const tagLen = view.getUint32(pos, true); pos += 4;
          const tag = readString(bytes, pos, tagLen);
          pos += tagLen;
          pos = (pos + 3) & ~3;
          handles.set(handle, document.createElement(tag));
          break;
        }
        case CMD_SET_TEXT: {
          const handle = view.getUint32(pos, true); pos += 4;
          const textLen = view.getUint32(pos, true); pos += 4;
          const text = readString(bytes, pos, textLen);
          pos += textLen;
          pos = (pos + 3) & ~3;
          const el = handles.get(handle);
          if (el) el.textContent = text;
          break;
        }
        case CMD_APPEND_CHILD: {
          const parent = view.getUint32(pos, true); pos += 4;
          const child = view.getUint32(pos, true); pos += 4;
          const parentEl = handles.get(parent);
          const childEl = handles.get(child);
          if (parentEl && childEl) parentEl.appendChild(childEl);
          break;
        }
        default:
          console.error(`phenotype: unknown command ${cmd} at offset ${pos - 4}`);
          return;
      }
    }
  };
}

// --- Loader ---

export async function mount(wasmUrl, rootElement = document.body) {
  // Late-binding reference: instance is set after instantiation,
  // but import functions capture it via closure.
  let inst = null;

  function getMemory() {
    return inst.exports.memory;
  }

  let executor = null;

  // WASI imports — each function captures `inst` via closure
  const wasiImports = {
    fd_write(fd, iovs_ptr, iovs_len, nwritten_ptr) {
      const mem = new DataView(getMemory().buffer);
      const bytes = new Uint8Array(getMemory().buffer);
      let total = 0;
      for (let i = 0; i < iovs_len; i++) {
        const ptr = mem.getUint32(iovs_ptr + i * 8, true);
        const len = mem.getUint32(iovs_ptr + i * 8 + 4, true);
        const chunk = bytes.slice(ptr, ptr + len);
        const text = new TextDecoder().decode(chunk);
        if (fd === 1) console.log(text);
        else if (fd === 2) console.error(text);
        total += len;
      }
      mem.setUint32(nwritten_ptr, total, true);
      return 0;
    },
    fd_close() { return 0; },
    fd_seek() { return 0; },
    fd_fdstat_get() { return 0; },
    fd_prestat_get() { return 8; },
    fd_prestat_dir_name() { return 8; },
    environ_get() { return 0; },
    environ_sizes_get(count_ptr, size_ptr) {
      const mem = new DataView(getMemory().buffer);
      mem.setUint32(count_ptr, 0, true);
      mem.setUint32(size_ptr, 0, true);
      return 0;
    },
    clock_time_get(id, precision, time_ptr) {
      const mem = new DataView(getMemory().buffer);
      const now = BigInt(Math.floor(performance.now() * 1e6));
      mem.setBigUint64(time_ptr, now, true);
      return 0;
    },
    proc_exit(code) {
      if (code !== 0) console.error(`proc_exit(${code})`);
    },
    sched_yield() { return 0; },
    random_get(buf_ptr, buf_len) {
      const bytes = new Uint8Array(getMemory().buffer, buf_ptr, buf_len);
      crypto.getRandomValues(bytes);
      return 0;
    },
  };

  // Phenotype imports
  const phenotypeImports = {
    flush() {
      if (executor) executor();
    },
  };

  const result = await WebAssembly.instantiateStreaming(
    fetch(wasmUrl),
    {
      wasi_snapshot_preview1: wasiImports,
      phenotype: phenotypeImports,
    }
  );
  inst = result.instance;
  executor = createExecutor(inst, rootElement);

  // Run WASI entry point
  if (inst.exports._start) {
    inst.exports._start();
  }
}
