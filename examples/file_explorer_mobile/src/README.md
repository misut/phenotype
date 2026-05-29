# File Explorer Mobile Source Guide

The mobile example mirrors the desktop example but keeps the phone layout in a
smaller set of C++ module partitions.

- `main.cpp` only enters the example.
- `file_explorer_mobile.cppm` imports phenotype, applies theme preferences, and
  imports the mobile partitions in dependency order.
- `file_explorer_mobile.messages_and_runtime.cppm` defines messages and runtime
  preferences.
- `file_explorer_mobile.state_and_debug.cppm` keeps mobile-only state helpers.
- `file_explorer_mobile.input_messages.cppm` maps UI messages to explorer input.
- `file_explorer_mobile.painting.cppm` paints mobile icons and thumbnails.
- `file_explorer_mobile.update.cppm` handles message updates.
- `file_explorer_mobile.views.cppm` composes the final mobile screens.
