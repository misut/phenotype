export module phenotype;

// Command buffer for batched DOM operations.
// C++ writes commands to a fixed region of linear memory,
// then calls flush() which invokes the JS-side executor.
// JS reads the buffer, interprets each command, and applies
// the corresponding DOM operations in one batch.

// Import from JS host — the shim provides this function.
// When called, JS reads the command buffer from WASM memory
// and executes all queued DOM operations.
extern "C" __attribute__((import_module("phenotype"), import_name("flush")))
void phenotype_flush(void);

// Exported symbols for JS to locate the command buffer in memory.
// These are read by the shim to find where commands are written.
extern "C" {
    // command buffer: 64KB fixed region
    alignas(4) unsigned char phenotype_cmd_buf[65536];
    // current write offset into the buffer
    unsigned int phenotype_cmd_len = 0;
    // next handle ID to assign (0 = document.body, reserved)
    unsigned int phenotype_next_handle = 1;

    // Accessor functions exported to JS (export_name only works on functions)
    __attribute__((export_name("phenotype_get_cmd_buf")))
    unsigned char* phenotype_get_cmd_buf(void) { return phenotype_cmd_buf; }

    __attribute__((export_name("phenotype_get_cmd_len")))
    unsigned int phenotype_get_cmd_len(void) { return phenotype_cmd_len; }
}

export namespace phenotype {

// DOM command opcodes
enum class Cmd : unsigned int {
    CreateElement = 1,  // tag_len, tag_bytes...        → assigns next handle
    SetText       = 2,  // handle, text_len, text_bytes...
    AppendChild   = 3,  // parent_handle, child_handle
};

namespace detail {

void write_u32(unsigned int value) {
    auto* p = reinterpret_cast<unsigned int*>(&phenotype_cmd_buf[phenotype_cmd_len]);
    *p = value;
    phenotype_cmd_len += 4;
}

void write_bytes(char const* data, unsigned int len) {
    for (unsigned int i = 0; i < len; ++i)
        phenotype_cmd_buf[phenotype_cmd_len++] = static_cast<unsigned char>(data[i]);
    // pad to 4-byte alignment
    while (phenotype_cmd_len % 4 != 0)
        phenotype_cmd_buf[phenotype_cmd_len++] = 0;
}

} // namespace detail

// Create a DOM element and return its handle.
// The element is not attached to the document until append_child() is called.
unsigned int create_element(char const* tag, unsigned int tag_len) {
    auto handle = phenotype_next_handle++;
    detail::write_u32(static_cast<unsigned int>(Cmd::CreateElement));
    detail::write_u32(handle);
    detail::write_u32(tag_len);
    detail::write_bytes(tag, tag_len);
    return handle;
}

// Set the text content of an element.
void set_text(unsigned int handle, char const* text, unsigned int text_len) {
    detail::write_u32(static_cast<unsigned int>(Cmd::SetText));
    detail::write_u32(handle);
    detail::write_u32(text_len);
    detail::write_bytes(text, text_len);
}

// Append child element to parent. Handle 0 = document.body.
void append_child(unsigned int parent, unsigned int child) {
    detail::write_u32(static_cast<unsigned int>(Cmd::AppendChild));
    detail::write_u32(parent);
    detail::write_u32(child);
}

// Flush all queued commands to the JS executor.
void flush() {
    if (phenotype_cmd_len == 0)
        return;
    phenotype_flush();
    phenotype_cmd_len = 0;
}

} // namespace phenotype
