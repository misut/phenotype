# Third-Party Notices

This repository embeds a small audited subset of SVG icon data from Google
Material Symbols (new) for toolbar, sidebar, and file-type glyphs in the
phenotype icon catalog. The glyphs are used as macOS/Finder-style semantic
references without embedding Apple, Finder, or SF Symbols artwork.

## Google Material Symbols

Source: https://github.com/google/material-design-icons

Embedded source revision:
`4d7678801370dc2fe9c35b437570f56f56e43801`

The runtime catalog reports 39 Google Material Symbols-backed semantic symbols
backed by 39 unique source icon names across the Outlined, Rounded, and Sharp
styles. Outlined is the default. File explorer package app icons also use the
pinned Material Symbols `folder_open` SVG source.

Embedded icon source files live in
`packages/phenotype_icon_contract/assets/material-symbols/{outlined,rounded,sharp}/`
and are normalized to `fill="currentColor"` so the renderer can tint them
through the same icon presentation path as the previous catalog.

License: Apache-2.0

The bundled license text is stored in:

- `packages/phenotype_icon_contract/assets/material-symbols/MATERIAL_SYMBOLS_LICENSE.txt`
- `examples/file_explorer_desktop/assets/icons/file-types/MATERIAL_SYMBOLS_LICENSE.txt`
- `examples/file_explorer_mobile/assets/icons/file-types/MATERIAL_SYMBOLS_LICENSE.txt`

Phenotype keeps the Google Material Symbols family, icon name, style, source
URL, license URL, source revision, and Apple-asset boundary in
`phenotype.icon_catalog` debug metadata so artifacts can be audited without
visual guessing.
