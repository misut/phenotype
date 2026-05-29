# File Explorer Shared Source Guide

The shared module owns the file explorer model, demo filesystem, CLI-driving
grammar, and debug JSON used by both desktop and mobile examples.

- `file_explorer_shared.cppm` is the module table of contents and re-exports
  the partitions below.
- `file_explorer_shared.model_types.cppm` defines the state, inputs, snapshots,
  and contracts.
- `file_explorer_shared.desktop_metrics_and_symbols.cppm` keeps desktop geometry
  and icon token contracts.
- `file_explorer_shared.viewport_and_focus_helpers.cppm` provides shared layout
  and focus helpers.
- `file_explorer_shared.chrome_and_geometry.cppm` computes toolbar/sidebar/file
  grid geometry for debug and artifact checks.
- `file_explorer_shared.filesystem_model.cppm` creates the demo tree and resolves
  file metadata.
- `file_explorer_shared.icon_and_interaction_debug_json.cppm`,
  `file_explorer_shared.chrome_debug_json.cppm`,
  `file_explorer_shared.resources_debug_json.cppm`, and
  `file_explorer_shared.debug_payload_json.cppm` build the debug payload.
- `file_explorer_shared.input_parsing.cppm` parses CLI/script inputs.
- `file_explorer_shared.filesystem_snapshot_helpers.cppm` records operations and
  builds snapshots.
- `file_explorer_shared.state_factories.cppm` creates initial explorer state.
- `file_explorer_shared.navigation_and_file_actions.cppm` implements file
  actions.
- `file_explorer_shared.focus_and_scripted_inputs.cppm` applies scripted inputs.
- `file_explorer_shared.drive_and_expectations.cppm` powers CLI verification.
- `file_explorer_shared.resources_and_labels.cppm` provides labels and resource
  catalog defaults.
