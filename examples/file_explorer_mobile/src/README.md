# File Explorer Mobile Source Guide

The mobile example mirrors the desktop example but keeps the phone layout in a
smaller set of chapters.

- `main.cpp` only enters the example.
- `file_explorer_mobile.cppm` imports phenotype, applies theme preferences, and
  includes the mobile chapters in dependency order.
- `file_explorer_mobile/messages_and_runtime.inc` defines messages and runtime
  preferences.
- `file_explorer_mobile/state_and_debug.inc` keeps mobile-only state helpers.
- `file_explorer_mobile/input_messages.inc` maps UI messages to explorer input.
- `file_explorer_mobile/painting.inc` paints mobile icons and thumbnails.
- `file_explorer_mobile/update.inc` handles message updates.
- `file_explorer_mobile/views.inc` composes the final mobile screens.
