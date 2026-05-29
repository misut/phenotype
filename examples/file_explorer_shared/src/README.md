# File Explorer Shared Source Guide

The shared module owns the file explorer model, demo filesystem, CLI-driving
grammar, and debug JSON used by both desktop and mobile examples.

- `file_explorer_shared.cppm` is the module table of contents.
- `file_explorer_shared/model_types.inc` defines the state, inputs, snapshots,
  and contracts.
- `file_explorer_shared/desktop_metrics_and_symbols.inc` keeps desktop geometry
  and icon token contracts.
- `file_explorer_shared/viewport_and_focus_helpers.inc` provides shared layout
  and focus helpers.
- `file_explorer_shared/chrome_and_geometry.inc` computes toolbar/sidebar/file
  grid geometry for debug and artifact checks.
- `file_explorer_shared/filesystem_model.inc` creates the demo tree and resolves
  file metadata.
- `file_explorer_shared/icon_and_interaction_debug_json.inc`,
  `chrome_debug_json.inc`, `resources_debug_json.inc`, and
  `debug_payload_json.inc` build the debug payload.
- `file_explorer_shared/input_parsing.inc` parses CLI/script inputs.
- `file_explorer_shared/filesystem_snapshot_helpers.inc` records operations and
  builds snapshots.
- `file_explorer_shared/state_factories.inc` creates initial explorer state.
- `file_explorer_shared/navigation_and_file_actions.inc` implements file
  actions.
- `file_explorer_shared/focus_and_scripted_inputs.inc` applies scripted inputs.
- `file_explorer_shared/drive_and_expectations.inc` powers CLI verification.
- `file_explorer_shared/resources_and_labels.inc` provides labels and resource
  catalog defaults.
