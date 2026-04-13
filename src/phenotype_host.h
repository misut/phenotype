// phenotype_host.h — Platform host function declarations.
//
// On wasm32-wasi, these are resolved by the JS shim via WebAssembly
// imports. On native, the application's backend provides them at
// link time (same signatures, no WASM attributes).
//
// Backend authors: implement all 5 functions to port phenotype to a
// new platform. See tests/test_phenotype.cpp for a minimal stub set.

#pragma once

#ifdef __wasi__
#define PHENOTYPE_IMPORT(mod, name) \
    __attribute__((import_module(mod), import_name(name)))
#else
#define PHENOTYPE_IMPORT(mod, name)
#endif

#ifdef __cplusplus
extern "C" {
#endif

PHENOTYPE_IMPORT("phenotype", "flush")
void phenotype_flush(void);

PHENOTYPE_IMPORT("phenotype", "measure_text")
float phenotype_measure_text(float font_size, unsigned int mono,
                             char const* text, unsigned int len);

PHENOTYPE_IMPORT("phenotype", "get_canvas_width")
float phenotype_get_canvas_width(void);

PHENOTYPE_IMPORT("phenotype", "get_canvas_height")
float phenotype_get_canvas_height(void);

PHENOTYPE_IMPORT("phenotype", "open_url")
void phenotype_open_url(char const* url, unsigned int len);

#ifdef __cplusplus
}
#endif

#undef PHENOTYPE_IMPORT
