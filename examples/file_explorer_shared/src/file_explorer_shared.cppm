module;
#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

export module file_explorer_shared;

import json;
import phenotype.icon_catalog;
import phenotype.resources;
import phenotype.theme_contract;

export namespace file_explorer_demo {
#include "file_explorer_shared/model_types.inc"
#include "file_explorer_shared/desktop_metrics_and_symbols.inc"
#include "file_explorer_shared/viewport_and_focus_helpers.inc"
#include "file_explorer_shared/chrome_and_geometry.inc"
#include "file_explorer_shared/filesystem_model.inc"
#include "file_explorer_shared/icon_and_interaction_debug_json.inc"
#include "file_explorer_shared/chrome_debug_json.inc"
#include "file_explorer_shared/resources_debug_json.inc"
#include "file_explorer_shared/debug_payload_json.inc"
#include "file_explorer_shared/input_parsing.inc"
#include "file_explorer_shared/filesystem_snapshot_helpers.inc"
#include "file_explorer_shared/state_factories.inc"
#include "file_explorer_shared/navigation_and_file_actions.inc"
#include "file_explorer_shared/focus_and_scripted_inputs.inc"
#include "file_explorer_shared/drive_and_expectations.inc"
#include "file_explorer_shared/resources_and_labels.inc"
} // namespace file_explorer_demo
