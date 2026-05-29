# File Explorer Desktop Source Guide

The desktop example is split into small chapters so new C++ readers can start
from `main.cpp` and then follow the UI in the order it is assembled.

- `main.cpp` only enters the example.
- `file_explorer_desktop.cppm` imports phenotype, wires the theme, creates the
  native window, and includes the chapters below in dependency order.
- `file_explorer_desktop/messages_and_startup.inc` defines CLI/startup messages.
- `file_explorer_desktop/input_messages.inc` defines UI messages.
- `file_explorer_desktop/runtime_preferences.inc` reads system and user display
  preferences.
- `file_explorer_desktop/state_and_debug.inc` keeps desktop-only state helpers.
- `file_explorer_desktop/style.inc` centralizes desktop colors and metrics.
- `file_explorer_desktop/painting_icons.inc` paints file and toolbar icons.
- `file_explorer_desktop/text_and_status.inc` formats labels and status text.
- `file_explorer_desktop/update.inc` handles message updates.
- `file_explorer_desktop/buttons_and_sidebar.inc` builds sidebar controls.
- `file_explorer_desktop/toolbar.inc` builds the toolbar.
- `file_explorer_desktop/overflow_and_grid.inc` builds menus and the file grid.
- `file_explorer_desktop/settings_and_views.inc` builds settings and secondary
  panels.
- `file_explorer_desktop/keyboard.inc` handles keyboard shortcuts.
- `file_explorer_desktop/view.inc` composes the final desktop view.
