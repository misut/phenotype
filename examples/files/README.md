# Files Example

This example is a small file explorer prototype built with `phenotype`.
It opens a macOS-only glass window and uses the native toolbar presets provided
by `phenotype` for navigation, view mode, sort, item actions, and expandable
search controls.

```sh
cd examples/files
mise exec -- exon build
```

Run the native app after building:

```sh
./.exon/debug/files
```
