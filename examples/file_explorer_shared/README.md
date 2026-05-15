# Shared File Explorer Model

This package exports the `file_explorer_shared` C++ module used by the
desktop and mobile file explorer examples.

The module owns the sandboxed file model, deterministic demo tree,
create/delete/duplicate/read/navigation behavior, sort mode state, selection
action metadata, operation receipts, and artifact startup scenarios. Keeping it
as a separate exon library makes the example UI packages depend on a normal
module contract instead of a relative header include.

## Run

```sh
cd examples/file_explorer_shared
mise exec -- exon test
```
