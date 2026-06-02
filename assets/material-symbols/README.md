Material Symbols assets
========================

The toolbar renders icons with the rounded Material Symbols variable font so
`FILL`, `GRAD`, `opsz`, and `wght` can be controlled from the native phenotype
interface.

Font source: https://github.com/google/material-design-icons
Font revision: 4d7678801370dc2fe9c35b437570f56f56e43801
Codepoints source: same repository and revision
License: Apache License 2.0
License URL: https://www.apache.org/licenses/LICENSE-2.0

The SVG files in `rounded/` are weight 400 fallbacks from
`@material-symbols/svg-400` version `0.44.11`. Their roots use
`fill="currentColor"` so AppKit can load them as template toolbar images and
tint them from the native control state when the font is unavailable.
