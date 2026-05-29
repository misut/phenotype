inline void record_operation(
        ExplorerState& state,
        std::string kind,
        std::string target,
        bool ok,
        std::string detail) {
    OperationPlan plan{
        .kind = kind,
        .display_name = target,
        .executable = ok,
        .sandboxed = ok,
        .fallback_reason = ok ? std::string{} : detail,
    };
    state.last_operation = OperationReceipt{
        .kind = std::move(kind),
        .target = std::move(target),
        .ok = ok,
        .detail = std::move(detail),
        .plan = std::move(plan),
    };
}

inline void record_operation(
        ExplorerState& state,
        OperationPlan plan,
        bool ok,
        std::string detail) {
    plan.executable = ok;
    if (!ok && plan.fallback_reason.empty())
        plan.fallback_reason = detail;
    auto target = plan.display_name;
    state.last_operation = OperationReceipt{
        .kind = plan.kind,
        .target = std::move(target),
        .ok = ok,
        .detail = std::move(detail),
        .plan = std::move(plan),
    };
}

inline std::string operation_label(OperationReceipt const& receipt) {
    if (receipt.kind.empty())
        return {};
    std::string label = "Operation: " + receipt.kind;
    label += receipt.ok ? " ok" : " failed";
    if (!receipt.target.empty())
        label += " - " + receipt.target;
    if (!receipt.detail.empty())
        label += " - " + receipt.detail;
    return label;
}

inline std::string relative_location(
        fs::path const& root,
        fs::path const& current,
        std::string_view root_label = "Demo Root") {
    auto root_name = root_label.empty()
        ? std::string{"Demo Root"}
        : std::string{root_label};
    std::error_code ec;
    auto const trash = trash_path(root);
    if (fs::equivalent(current, trash, ec))
        return "Trash";
    ec.clear();
    auto trash_rel = fs::relative(current, trash, ec);
    if (!ec && !trash_rel.empty() && trash_rel != ".") {
        auto text = trash_rel.generic_string();
        if (text != ".." && !text.starts_with("../"))
            return std::string("Trash/") + text;
    }
    ec.clear();
    auto rel = fs::relative(current, root, ec);
    if (ec || rel.empty() || rel == ".")
        return root_name;
    return root_name + "/" + rel.generic_string();
}

inline std::string relative_location(ExplorerState const& state) {
    return relative_location(state.root, state.current, state.root_label);
}

inline bool same_path(fs::path const& a, fs::path const& b) {
    if (a.empty() || b.empty())
        return a.empty() == b.empty();

    std::error_code ec;
    if (fs::equivalent(a, b, ec))
        return true;

    auto lhs = a.lexically_normal().generic_string();
    auto rhs = b.lexically_normal().generic_string();
#if defined(_WIN32)
    lhs = lower_copy(std::move(lhs));
    rhs = lower_copy(std::move(rhs));
#endif
    return lhs == rhs;
}

inline void trim_history(std::vector<fs::path>& history) {
    constexpr std::size_t max_history = 32;
    if (history.size() > max_history) {
        history.erase(history.begin(),
                      history.begin()
                          + static_cast<std::ptrdiff_t>(
                              history.size() - max_history));
    }
}

inline void remember_current_for_back(ExplorerState& state) {
    if (state.current.empty())
        return;
    if (state.back_stack.empty()
        || !same_path(state.back_stack.back(), state.current)) {
        state.back_stack.push_back(state.current);
        trim_history(state.back_stack);
    }
    state.forward_stack.clear();
}

inline void open_directory(
        ExplorerState& state,
        fs::path next,
        std::string status,
        bool record_history = true) {
    if (!same_path(state.current, next)) {
        if (record_history)
            remember_current_for_back(state);
        state.current = std::move(next);
    }
    state.selected_name.clear();
    state.status = std::move(status);
}

inline bool matches_filter(std::string const& name, std::string const& filter) {
    auto needle = trim(filter);
    if (needle.empty())
        return true;
    return lower_copy(name).find(lower_copy(needle)) != std::string::npos;
}

inline bool protected_root_folder(std::string_view name) {
    return name == "Documents" || name == "Pictures" || name == "Shared"
        || name == ".Trash";
}

inline int finder_recents_rank(std::string_view name) {
    if (name == "작성예시3_선택_DC 탈퇴신청서.pdf")
        return 0;
    if (name == "작성예시2_필수_운용지시서.pdf")
        return 1;
    if (name == "작성예시1_필수_중도인출 신청서.pdf")
        return 2;
    if (name == "2_필수_운용지시서.pdf")
        return 3;
    if (name == "1_필수_중도인출 신청서.pdf")
        return 4;
    if (name == "※해당 시 필독_①무주택자인 경우.pdf")
        return 5;
    if (name == "[카카오] 퇴직금 지급 기준.pdf")
        return 6;
    if (name == "line_2.png")
        return 7;
    if (name == "line_23.png")
        return 8;
    if (name == "line_8.png")
        return 9;
    if (name == "line_4.png")
        return 10;
    if (name == "Screen Recording 2025-11-04.mov")
        return 11;
    if (name == "Screen Recording 2025-11-05.mov")
        return 12;
    if (name == "Screen Recording 2025-11-06.mov")
        return 13;
    if (name == "Screen Recording 2025-11-07.mov")
        return 14;
    if (name == "Screenshot 2025-11-05.png")
        return 15;
    if (name == "Screenshot 2025-11-06.png")
        return 16;
    if (name == "Glass Symbol.svg")
        return 17;
    if (name == "README.txt")
        return 18;
    if (name == "Application Form 3.pdf")
        return 19;
    if (name == "Application Form 2.pdf")
        return 20;
    if (name == "Archive.zip")
        return 21;
    if (name == "契約書_サンプル.pdf")
        return 22;
    if (name == "资产证明.pdf")
        return 23;
    if (name == "Voice Memo.m4a")
        return 24;
    if (name == "Glass Theme Notes.cpp")
        return 25;
    if (name == "Expense Matrix.csv")
        return 26;
    if (name == "Design Review.key")
        return 27;
    return 1000;
}

inline bool entry_name_less(Entry const& a, Entry const& b) {
    return lower_copy(a.name) < lower_copy(b.name);
}

inline bool entry_less(Entry const& a, Entry const& b, SortMode sort_mode) {
    if (sort_mode == SortMode::Recent) {
        auto lhs_rank = finder_recents_rank(a.name);
        auto rhs_rank = finder_recents_rank(b.name);
        if (lhs_rank != rhs_rank)
            return lhs_rank < rhs_rank;
        if (a.folder != b.folder)
            return !a.folder && b.folder;
        return entry_name_less(a, b);
    }
    if (a.folder != b.folder)
        return a.folder && !b.folder;
    if (sort_mode == SortMode::Kind) {
        auto lhs_kind = lower_copy(entry_kind_label(a));
        auto rhs_kind = lower_copy(entry_kind_label(b));
        if (lhs_kind != rhs_kind)
            return lhs_kind < rhs_kind;
    } else if (sort_mode == SortMode::Size && !a.folder && !b.folder) {
        if (a.size != b.size)
            return a.size > b.size;
    }
    return entry_name_less(a, b);
}

inline void sort_entries(std::vector<Entry>& entries, SortMode sort_mode) {
    for (std::size_t i = 1; i < entries.size(); ++i) {
        auto value = std::move(entries[i]);
        auto j = i;
        while (j > 0 && entry_less(value, entries[j - 1], sort_mode)) {
            entries[j] = std::move(entries[j - 1]);
            --j;
        }
        entries[j] = std::move(value);
    }
}

inline bool path_inside_root(fs::path const& root, fs::path const& path) {
    auto rel = path.lexically_relative(root).generic_string();
    if (rel.empty() || rel == ".")
        return false;
    return rel != ".." && !rel.starts_with("../");
}

inline bool direct_child_entry_name(std::string_view name) {
    auto text = trim(name);
    if (text.empty() || text != name || text == "." || text == "..")
        return false;
    if (text.front() == '.')
        return false;
    return text.find('/') == std::string::npos
        && text.find('\\') == std::string::npos;
}

inline std::optional<fs::path> direct_child_entry_path(
        ExplorerState const& state,
        std::string_view name) {
    if (!direct_child_entry_name(name))
        return std::nullopt;
    auto path = (state.current / std::string{name}).lexically_normal();
    if (!path_inside_root(state.root, path))
        return std::nullopt;
    return path;
}

inline bool deletable_directory(
        fs::path const& root,
        fs::path const& path) {
    std::error_code ec;
    if (!fs::is_directory(path, ec) || ec)
        return false;
    if (!path_inside_root(root, path) || same_path(root, path))
        return false;
    if (same_path(path.parent_path(), root)
        && protected_root_folder(path.filename().string())) {
        return false;
    }
    return true;
}

inline std::string operation_path_label(
        ExplorerState const& state,
        fs::path const& path) {
    if (path.empty())
        return {};
    auto const normalized = path.lexically_normal();
    auto const rel = normalized.lexically_relative(state.root).generic_string();
    auto root_name = state.root_label.empty()
        ? std::string{"Demo Root"}
        : state.root_label;
    if (rel.empty() || rel == ".")
        return root_name;
    if (rel == ".." || rel.starts_with("../"))
        return "[outside-demo-root]";
    if (rel == ".Trash")
        return "Trash";
    if (rel.starts_with(".Trash/"))
        return std::string{"Trash/"} + rel.substr(7);
    return root_name + "/" + rel;
}

inline OperationPlan planned_file_create(
        ExplorerState const& state,
        std::string const& display_name,
        fs::path const& target) {
    return OperationPlan{
        .kind = "file_create",
        .display_name = display_name,
        .destination = operation_path_label(state, target),
        .executable = true,
        .sandboxed = state.root_is_demo && path_inside_root(state.root, target),
        .mutates_filesystem = true,
        .writes_file = true,
    };
}

inline OperationPlan planned_folder_create(
        ExplorerState const& state,
        std::string const& display_name,
        fs::path const& target) {
    return OperationPlan{
        .kind = "folder_create",
        .display_name = display_name,
        .destination = operation_path_label(state, target),
        .executable = true,
        .sandboxed = state.root_is_demo && path_inside_root(state.root, target),
        .mutates_filesystem = true,
        .creates_directory = true,
    };
}

inline OperationPlan planned_entry_read(
        ExplorerState const& state,
        std::string const& display_name,
        fs::path const& source,
        bool folder) {
    return OperationPlan{
        .kind = folder ? "folder_select" : "file_read",
        .display_name = display_name,
        .source = operation_path_label(state, source),
        .executable = true,
        .sandboxed = state.root_is_demo && path_inside_root(state.root, source),
        .reads_file = !folder,
        .reads_directory = folder,
    };
}

inline OperationPlan planned_folder_open(
        ExplorerState const& state,
        std::string const& display_name,
        fs::path const& source) {
    return OperationPlan{
        .kind = "folder_open",
        .display_name = display_name,
        .source = operation_path_label(state, source),
        .destination = operation_path_label(state, source),
        .executable = true,
        .sandboxed = state.root_is_demo && path_inside_root(state.root, source),
        .reads_directory = true,
    };
}

inline OperationPlan planned_delete(
        ExplorerState const& state,
        std::string const& display_name,
        fs::path const& source,
        bool folder,
        bool deleting_from_trash,
        fs::path const& destination = {}) {
    return OperationPlan{
        .kind = folder ? "folder_delete" : "file_delete",
        .display_name = display_name,
        .source = operation_path_label(state, source),
        .destination = operation_path_label(state, destination),
        .executable = true,
        .sandboxed = state.root_is_demo
            && path_inside_root(state.root, source)
            && (destination.empty() || path_inside_root(state.root, destination)),
        .mutates_filesystem = true,
        .deletes_file = !folder,
        .deletes_directory = folder,
        .moves_to_trash = !deleting_from_trash,
        .permanent_delete = deleting_from_trash,
    };
}

inline OperationPlan planned_duplicate(
        ExplorerState const& state,
        std::string const& display_name,
        fs::path const& source,
        fs::path const& destination) {
    return OperationPlan{
        .kind = "file_duplicate",
        .display_name = display_name,
        .source = operation_path_label(state, source),
        .destination = operation_path_label(state, destination),
        .executable = true,
        .sandboxed = state.root_is_demo
            && path_inside_root(state.root, source)
            && path_inside_root(state.root, destination),
        .mutates_filesystem = true,
        .reads_file = true,
        .writes_file = true,
    };
}

inline std::vector<Entry> list_entries(
        fs::path const& current,
        std::string const& filter,
        SortMode sort_mode = SortMode::Name) {
    std::vector<Entry> entries;
    std::error_code ec;
    fs::directory_iterator it(current, ec);
    if (ec)
        return entries;
    for (auto const& item : it) {
        auto name = item.path().filename().string();
        if (!name.empty() && name.front() == '.')
            continue;
        if (name.empty() || !matches_filter(name, filter))
            continue;
        Entry entry;
        entry.name = name;
        ec.clear();
        entry.folder = item.is_directory(ec);
        if (ec)
            continue;
        if (!entry.folder) {
            ec.clear();
            if (!item.is_regular_file(ec) || ec)
                continue;
            ec.clear();
            entry.size = item.file_size(ec);
            if (ec)
                entry.size = 0;
        }
        entries.push_back(entry);
    }
    sort_entries(entries, sort_mode);
    return entries;
}

inline std::string read_preview(fs::path const& path) {
    std::error_code ec;
    if (!fs::exists(path, ec))
        return "No file selected.";
    if (fs::is_directory(path, ec))
        return "Open this folder to view its files.";
    auto size = fs::file_size(path, ec);
    if (ec)
        return "Unable to read file size.";
    if (size > 64 * 1024)
        return "Preview skipped because the file is larger than 64 KB.";

    std::ifstream in(path, std::ios::binary);
    if (!in)
        return "Unable to open file for preview.";
    std::ostringstream buffer;
    buffer << in.rdbuf();
    auto text = buffer.str();
    for (char& ch : text) {
        unsigned char uch = static_cast<unsigned char>(ch);
        if (ch == '\n' || ch == '\t')
            continue;
        if (uch < 32 || uch == 127)
            ch = ' ';
    }
    if (text.empty())
        return "(empty file)";
    if (text.size() > 1800)
        text = text.substr(0, 1800) + "\n...";
    return text;
}

