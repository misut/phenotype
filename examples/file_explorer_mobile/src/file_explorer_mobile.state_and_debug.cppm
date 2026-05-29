module;
#include <algorithm>
#include <cctype>
#include <concepts>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

export module file_explorer_mobile:state_and_debug;

import file_explorer_shared;
import phenotype;
import phenotype.native;
import :messages_and_runtime;

export namespace file_explorer_mobile {
struct State {
    phenotype::icons::SymbolDocumentCache icon_cache =
        phenotype::icons::make_symbol_document_cache();
    file_explorer_demo::ExplorerState explorer;
    file_explorer_demo::ExplorerLabels labels;

    State()
        : explorer(initial_explorer_state()),
          labels(file_explorer_demo::file_explorer_labels(
              resolved_initial_locale(),
              runtime_resource_catalog())) {
        file_explorer_demo::apply_runtime_preferences(
            explorer,
            g_runtime_preferences);
        apply_startup_inputs(explorer, "mobile");
        sync_runtime_theme(explorer);
    }
};

State const* g_debug_state = nullptr;

auto file_explorer_application_debug_payload() {
    if (!g_debug_state) {
        file_explorer_demo::ExplorerState empty{};
        auto snap = file_explorer_demo::snapshot(empty);
        auto chrome = file_explorer_demo::explorer_chrome_metrics(empty, "mobile");
        return file_explorer_demo::file_explorer_application_debug_json(
            empty,
            snap,
            chrome,
            "mobile");
    }
    auto snap = file_explorer_demo::snapshot(g_debug_state->explorer);
    auto chrome = file_explorer_demo::explorer_chrome_metrics(
        g_debug_state->explorer,
        "mobile");
    return file_explorer_demo::file_explorer_application_debug_json(
        g_debug_state->explorer,
        snap,
        chrome,
        "mobile");
}

} // namespace file_explorer_mobile
