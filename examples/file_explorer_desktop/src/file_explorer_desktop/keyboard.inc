void finder_key_command(phenotype::native::Key key,
                        int modifiers,
                        CommandMsg msg,
                        char const* label,
                        bool allow_when_input_focused = false) {
    phenotype::app::key_command<Msg>(
        static_cast<unsigned int>(key),
        modifiers,
        Msg{std::move(msg)},
        phenotype::KeyCommandOptions{
            .allow_when_input_focused = allow_when_input_focused,
            .debug_label = label ? std::string{label} : std::string{},
        });
}

ExplorerInputMessage explorer_key_message(
        file_explorer_demo::ExplorerInput input) {
    input.modality = file_explorer_demo::ExplorerInputModality::Keyboard;
    return ExplorerInputMessage{std::move(input)};
}

ExplorerInputMessage explorer_key_message(
        file_explorer_demo::ExplorerInputKind kind) {
    return explorer_key_message(keyboard_input(kind));
}

void finder_key_input(phenotype::native::Key key,
                      int modifiers,
                      file_explorer_demo::ExplorerInput input,
                      char const* label,
                      bool allow_when_input_focused = false) {
    finder_key_command(
        key,
        modifiers,
        explorer_key_message(std::move(input)),
        label,
        allow_when_input_focused);
}

void finder_key_input(phenotype::native::Key key,
                      int modifiers,
                      file_explorer_demo::ExplorerInputKind kind,
                      char const* label,
                      bool allow_when_input_focused = false) {
    finder_key_input(
        key,
        modifiers,
        keyboard_input(kind),
        label,
        allow_when_input_focused);
}

void finder_key_move(phenotype::native::Key key,
                     int modifiers,
                     file_explorer_demo::ExplorerSelectionMove move,
                     char const* label) {
    finder_key_input(key, modifiers, keyboard_move_input(move), label);
}

void register_finder_key_commands() {
    using phenotype::native::Key;
    using phenotype::native::Modifier;
    constexpr int none = 0;
    constexpr int shift = static_cast<int>(Modifier::Shift);
    constexpr int control = static_cast<int>(Modifier::Control);
    constexpr int super = static_cast<int>(Modifier::Super);

    finder_key_command(Key::F, super, ShowSearch{}, "show_search", true);
    finder_key_command(Key::F, control, ShowSearch{}, "show_search", true);
    finder_key_input(
        Key::Enter,
        none,
        file_explorer_demo::ExplorerInputKind::ActivateSelected,
        "activate_selected");
    finder_key_input(
        Key::KpEnter,
        none,
        file_explorer_demo::ExplorerInputKind::ActivateSelected,
        "activate_selected");
    finder_key_move(
        Key::Up,
        none,
        file_explorer_demo::ExplorerSelectionMove::Up,
        "move_selection_up");
    finder_key_move(
        Key::Down,
        none,
        file_explorer_demo::ExplorerSelectionMove::Down,
        "move_selection_down");
    finder_key_move(
        Key::Left,
        none,
        file_explorer_demo::ExplorerSelectionMove::Left,
        "move_selection_left");
    finder_key_move(
        Key::Right,
        none,
        file_explorer_demo::ExplorerSelectionMove::Right,
        "move_selection_right");
    finder_key_move(
        Key::Home,
        none,
        file_explorer_demo::ExplorerSelectionMove::Home,
        "move_selection_home");
    finder_key_move(
        Key::End,
        none,
        file_explorer_demo::ExplorerSelectionMove::End,
        "move_selection_end");
    finder_key_move(
        Key::PageUp,
        none,
        file_explorer_demo::ExplorerSelectionMove::PageUp,
        "move_selection_page_up");
    finder_key_move(
        Key::PageDown,
        none,
        file_explorer_demo::ExplorerSelectionMove::PageDown,
        "move_selection_page_down");
    finder_key_input(
        Key::Backspace,
        none,
        file_explorer_demo::ExplorerInputKind::DeleteSelected,
        "delete_selected");
    finder_key_input(
        Key::Delete,
        none,
        file_explorer_demo::ExplorerInputKind::DeleteSelected,
        "delete_selected");
    finder_key_input(
        Key::Backspace,
        super,
        file_explorer_demo::ExplorerInputKind::DeleteSelected,
        "delete_selected");
    finder_key_input(
        Key::Backspace,
        control,
        file_explorer_demo::ExplorerInputKind::DeleteSelected,
        "delete_selected");
    finder_key_input(
        Key::Delete,
        super,
        file_explorer_demo::ExplorerInputKind::DeleteSelected,
        "delete_selected");
    finder_key_input(
        Key::Delete,
        control,
        file_explorer_demo::ExplorerInputKind::DeleteSelected,
        "delete_selected");
    finder_key_input(
        Key::D,
        super,
        file_explorer_demo::ExplorerInputKind::DuplicateSelected,
        "duplicate_selected");
    finder_key_input(
        Key::D,
        control,
        file_explorer_demo::ExplorerInputKind::DuplicateSelected,
        "duplicate_selected");
    finder_key_input(
        Key::N,
        shift | super,
        file_explorer_demo::ExplorerInputKind::CreateFolder,
        "create_folder");
    finder_key_input(
        Key::N,
        shift | control,
        file_explorer_demo::ExplorerInputKind::CreateFolder,
        "create_folder");
    finder_key_input(
        Key::Escape,
        none,
        file_explorer_demo::ExplorerInputKind::DismissTransient,
        "dismiss_transient",
        true);
}

