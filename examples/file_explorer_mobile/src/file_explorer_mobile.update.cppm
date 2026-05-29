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

export module file_explorer_mobile:update;

import file_explorer_shared;
import phenotype;
import phenotype.native;
import :messages_and_runtime;
import :state_and_debug;
import :input_messages;
import :painting;

export namespace file_explorer_mobile {
void update(State& state, Msg msg) {
    auto& explorer = state.explorer;
    std::visit([&](auto const& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::same_as<T, SelectLocation>) {
            file_explorer_demo::select_location(explorer, m.id);
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, SelectEntry>) {
            file_explorer_demo::open_entry(explorer, m.name);
            if (!explorer.selected_name.empty())
                explorer.mobile_tab = 1;
        } else if constexpr (std::same_as<T, ExplorerInputMessage>) {
            file_explorer_demo::apply_explorer_input(
                explorer,
                m.input,
                "mobile");
        } else if constexpr (std::same_as<T, SearchChanged>) {
            file_explorer_demo::set_search_filter(explorer, m.text);
        } else if constexpr (std::same_as<T, DraftNameChanged>) {
            explorer.draft_name = m.text;
        } else if constexpr (std::same_as<T, DraftFolderNameChanged>) {
            explorer.draft_folder_name = m.text;
        } else if constexpr (std::same_as<T, DraftBodyChanged>) {
            explorer.draft_body = m.text;
        } else if constexpr (std::same_as<T, SelectTab>) {
            explorer.mobile_tab = m.value;
        } else if constexpr (std::same_as<T, CreateFile>) {
            file_explorer_demo::create_file(explorer);
            explorer.mobile_tab = 1;
        } else if constexpr (std::same_as<T, CreateFolder>) {
            file_explorer_demo::create_folder(explorer);
            explorer.mobile_tab = 1;
        } else if constexpr (std::same_as<T, DeleteSelected>) {
            file_explorer_demo::delete_selected(explorer);
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, DuplicateSelected>) {
            file_explorer_demo::duplicate_selected(explorer);
            explorer.mobile_tab = 1;
        } else if constexpr (std::same_as<T, GoBack>) {
            file_explorer_demo::go_back(explorer);
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, GoForward>) {
            file_explorer_demo::go_forward(explorer);
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, GoUp>) {
            file_explorer_demo::go_up(explorer);
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, CycleSort>) {
            file_explorer_demo::cycle_sort_mode(explorer);
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, ResetDemo>) {
            file_explorer_demo::reset_demo_tree(explorer, "mobile");
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, Resized>) {
            explorer.viewport_width = m.width;
            explorer.viewport_height = m.height;
            explorer.viewport_scale = m.scale;
        }
    }, msg);
    sync_runtime_theme(explorer);
}

} // namespace file_explorer_mobile
