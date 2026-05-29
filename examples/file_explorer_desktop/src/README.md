# File Explorer Desktop Source Guide

The desktop example is split into small C++ module partitions so new C++
readers can start from `main.cpp` and then follow the UI in the order it is
assembled.

- `main.cpp` only enters the example.
- `file_explorer_desktop.cppm` imports phenotype, wires the theme, creates the
  native window, and imports the partitions below in dependency order.
- `file_explorer_desktop.messages_and_startup.cppm` defines CLI/startup messages.
- `file_explorer_desktop.input_messages.cppm` defines UI messages.
- `file_explorer_desktop.runtime_preferences.cppm` reads system and user display
  preferences.
- `file_explorer_desktop.state_and_debug.cppm` keeps desktop-only state helpers.
- `file_explorer_desktop.style.cppm` centralizes desktop colors and metrics.
- `file_explorer_desktop.painting_icons.cppm` paints file and toolbar icons.
- `file_explorer_desktop.text_and_status.cppm` formats labels and status text.
- `file_explorer_desktop.update.cppm` handles message updates.
- `file_explorer_desktop.buttons_and_sidebar.cppm` builds sidebar controls.
- `file_explorer_desktop.toolbar.cppm` builds the toolbar.
- `file_explorer_desktop.overflow_and_grid.cppm` builds menus and the file grid.
- `file_explorer_desktop.settings_and_views.cppm` builds settings and secondary
  panels.
- `file_explorer_desktop.keyboard.cppm` handles keyboard shortcuts.
- `file_explorer_desktop.view.cppm` composes the final desktop view.
