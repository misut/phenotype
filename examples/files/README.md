# Files Example

This example is a small file explorer prototype built with `phenotype`.
The current pairing baseline opens an empty macOS-only glass window. The window
uses an AppKit compositor backdrop so content behind the window is softened by a
frosted blur instead of showing through as readable content.
The top toolbar benchmarks Finder's icon-first chrome and uses vendored Google
Material Symbols resources provided by `phenotype` for navigation, view mode,
sort, share, tag, more, and search controls.

```sh
cd examples/files
mise exec -- exon build
```

Run the native app after building:

```sh
./.exon/debug/files
```
