export module phenotype;

// Command buffer for batched DOM operations.
// C++ writes commands to a fixed region of linear memory,
// then calls flush() which invokes the JS-side executor.
// JS reads the buffer, interprets each command, and applies
// the corresponding DOM operations in one batch.

extern "C" __attribute__((import_module("phenotype"), import_name("flush")))
void phenotype_flush(void);

extern "C" {
    alignas(4) unsigned char phenotype_cmd_buf[65536];
    unsigned int phenotype_cmd_len = 0;
    unsigned int phenotype_next_handle = 1; // 0 = document.body

    __attribute__((export_name("phenotype_get_cmd_buf")))
    unsigned char* phenotype_get_cmd_buf(void) { return phenotype_cmd_buf; }

    __attribute__((export_name("phenotype_get_cmd_len")))
    unsigned int phenotype_get_cmd_len(void) { return phenotype_cmd_len; }
}

constexpr unsigned int BUF_SIZE = 65536;

export namespace phenotype {

// DOM command opcodes — must match shim/phenotype.js
enum class Cmd : unsigned int {
    CreateElement = 1,
    SetText       = 2,
    AppendChild   = 3,
    SetAttribute  = 4,
    SetStyle      = 5,
    SetInnerHTML  = 6,
    AddClass      = 7,
    RemoveChild   = 8,
};

namespace detail {

// Flush if the buffer cannot fit `needed` more bytes.
void ensure(unsigned int needed) {
    if (phenotype_cmd_len + needed > BUF_SIZE) {
        phenotype_flush();
        phenotype_cmd_len = 0;
    }
}

void write_u32(unsigned int value) {
    auto* p = reinterpret_cast<unsigned int*>(&phenotype_cmd_buf[phenotype_cmd_len]);
    *p = value;
    phenotype_cmd_len += 4;
}

void write_bytes(char const* data, unsigned int len) {
    for (unsigned int i = 0; i < len; ++i)
        phenotype_cmd_buf[phenotype_cmd_len++] = static_cast<unsigned char>(data[i]);
    while (phenotype_cmd_len % 4 != 0)
        phenotype_cmd_buf[phenotype_cmd_len++] = 0;
}

// Padded size of a byte string (rounded up to 4-byte alignment)
unsigned int padded(unsigned int len) {
    return (len + 3) & ~3u;
}

} // namespace detail

void flush() {
    if (phenotype_cmd_len == 0)
        return;
    phenotype_flush();
    phenotype_cmd_len = 0;
}

unsigned int create_element(char const* tag, unsigned int tag_len) {
    detail::ensure(12 + detail::padded(tag_len));
    auto handle = phenotype_next_handle++;
    detail::write_u32(static_cast<unsigned int>(Cmd::CreateElement));
    detail::write_u32(handle);
    detail::write_u32(tag_len);
    detail::write_bytes(tag, tag_len);
    return handle;
}

void set_text(unsigned int handle, char const* text, unsigned int text_len) {
    detail::ensure(12 + detail::padded(text_len));
    detail::write_u32(static_cast<unsigned int>(Cmd::SetText));
    detail::write_u32(handle);
    detail::write_u32(text_len);
    detail::write_bytes(text, text_len);
}

void append_child(unsigned int parent, unsigned int child) {
    detail::ensure(12);
    detail::write_u32(static_cast<unsigned int>(Cmd::AppendChild));
    detail::write_u32(parent);
    detail::write_u32(child);
}

void set_attribute(unsigned int handle,
                   char const* key, unsigned int key_len,
                   char const* val, unsigned int val_len) {
    detail::ensure(16 + detail::padded(key_len) + detail::padded(val_len));
    detail::write_u32(static_cast<unsigned int>(Cmd::SetAttribute));
    detail::write_u32(handle);
    detail::write_u32(key_len);
    detail::write_bytes(key, key_len);
    detail::write_u32(val_len);
    detail::write_bytes(val, val_len);
}

void set_style(unsigned int handle,
               char const* prop, unsigned int prop_len,
               char const* val, unsigned int val_len) {
    detail::ensure(16 + detail::padded(prop_len) + detail::padded(val_len));
    detail::write_u32(static_cast<unsigned int>(Cmd::SetStyle));
    detail::write_u32(handle);
    detail::write_u32(prop_len);
    detail::write_bytes(prop, prop_len);
    detail::write_u32(val_len);
    detail::write_bytes(val, val_len);
}

void set_inner_html(unsigned int handle, char const* html, unsigned int html_len) {
    detail::ensure(12 + detail::padded(html_len));
    detail::write_u32(static_cast<unsigned int>(Cmd::SetInnerHTML));
    detail::write_u32(handle);
    detail::write_u32(html_len);
    detail::write_bytes(html, html_len);
}

void add_class(unsigned int handle, char const* cls, unsigned int cls_len) {
    detail::ensure(12 + detail::padded(cls_len));
    detail::write_u32(static_cast<unsigned int>(Cmd::AddClass));
    detail::write_u32(handle);
    detail::write_u32(cls_len);
    detail::write_bytes(cls, cls_len);
}

void remove_child(unsigned int parent, unsigned int child) {
    detail::ensure(12);
    detail::write_u32(static_cast<unsigned int>(Cmd::RemoveChild));
    detail::write_u32(parent);
    detail::write_u32(child);
}

} // namespace phenotype
