#include "main.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#endif

#include "config/config.hpp"
#include "bookmarks/bookmarks.hpp"
#include "browser/browser.hpp"
#include "help/help.hpp"
#include "history/history.hpp"
#include "recent/recent.hpp"
#include "links/links.hpp"
#include "links/brokelinks.hpp"
#include "op/op.hpp"
#include "search/search.hpp"
#include "undo/undo.hpp"

using namespace ftxui;

int main(int, char** argv) {
    auto state = std::make_shared<EditorState>();

    auto editor_comp = terminadventure::editor::MakeEditor(state);
    auto treeview_comp = terminadventure::treeview::MakeTreeView(state);

    state->focus_editor = [editor_comp] { editor_comp->TakeFocus(); };
    state->focus_treeview = [treeview_comp] { treeview_comp->TakeFocus(); };

    auto treeview_wrap = treeview_comp;
    auto editor_wrap = editor_comp;

    auto main_split = ResizableSplitLeft(treeview_wrap, editor_wrap, &state->treeview_width);

    auto screen = ScreenInteractive::Fullscreen();
    auto quit = screen.ExitLoopClosure();

#ifndef _WIN32
    struct termios term;
    if (tcgetattr(STDIN_FILENO, &term) == 0)
    {
        term.c_lflag &= ~ISIG;
        tcsetattr(STDIN_FILENO, TCSANOW, &term);
    }
#endif

    state->operations["quit"] = [state, quit](const std::string&, int)
    {
        if (state->changed)
        {
            state->status = "Unsaved changes - use :w to save or :qa! to quit without saving";
            return;
        }
        quit();
    };
    state->operations["quit_force"] = [quit](const std::string&, int) { quit(); };
    state->commands["qa"] = "quit";
    state->commands["qa!"] = "quit_force";

    bool show_help = false;
    state->operations["show_help"] = [&show_help](const std::string&, int) { show_help = true; };

    bool show_search = false;
    state->operations["search_start"] = [&show_search](const std::string&, int) { show_search = true; };

    bool show_link_picker = false;
    state->operations["insert_link"] = [&show_link_picker](const std::string&, int) { show_link_picker = true; };

    bool show_bookmarks = false;
    state->operations["bookmarks"] = [&show_bookmarks](const std::string&, int) { show_bookmarks = true; };
    state->commands["bookmarks"] = "bookmarks";

    bool show_links = false;
    state->operations["links"] = [&show_links](const std::string&, int) { show_links = true; };
    state->commands["links"] = "links";

    bool show_dead_links = false;
    state->operations["show_broke_links"] = [&show_dead_links](const std::string&, int) { show_dead_links = true; };
    state->commands["show_broke_links"] = "show_broke_links";

    bool show_history = false;
    state->operations["history"] = [&show_history](const std::string&, int) { show_history = true; };

    bool show_recent = false;
    auto recent_force = std::make_shared<bool>(false);
    const auto open_recent = [&show_recent, &state, &recent_force](bool force)
    {
        const std::size_t removed = terminadventure::recent::PruneRecentFiles(state->recent_files);
        if (removed > 0)
        {
            if (!state->init_path.empty())
            {
                terminadventure::config::WriteRecentFiles(state->init_path, state->recent_files);
            }
            state->status = "Removed " + std::to_string(removed) +
                " stale recent entr" + (removed == 1 ?"y" : "ies");
        }
        *recent_force = force;
        show_recent = true;
    };
    state->operations["recents"] = [&open_recent](const std::string&, int) { open_recent(false); };
    state->operations["recents_force"] = [&open_recent](const std::string&, int) { open_recent(true); };

    bool show_undo = false;
    state->operations["undo"] = [&show_undo](const std::string&, int) { show_undo = true; };

    bool show_file_browser = false;
    state->show_file_browser = &show_file_browser;

    namespace fs = std::filesystem;
    fs::path config_path = fs::path(argv[0]).parent_path() / "commands.conf";
    if (!fs::exists(config_path))
    {
        config_path = "commands.conf";
    }
    if (!terminadventure::config::LoadConfig(config_path.string(), state))
    {
        std::cerr << "Warning: could not load config from " << config_path.string() << "\n";
        std::cerr << "Only the built-in ':qa' command is available.\n";
    }

    fs::path template_path = fs::path(argv[0]).parent_path() / "terminadventure.html";
    if (!fs::exists(template_path))
    {
        template_path = "terminadventure.html";
    }
    state->template_path = template_path.string();

    fs::path init_path;
    if (const char* env = std::getenv("TERMINADVENTURE_INIT"); env != nullptr && *env != '\0')
    {
        init_path = env;
    }
    else
    {
        init_path = fs::path(argv[0]).parent_path() / "init.conf";
    }
    state->init_path = init_path.string();

    std::string last_file;
    if (terminadventure::config::ReadInit(init_path.string(), last_file, state->recent_files) && !last_file.empty())
    {
        auto it = state->operations.find("open");
        std::error_code ec;
        if (it != state->operations.end() && fs::exists(last_file, ec))
        {
            it->second(last_file, 1);
        }
    }

    if (std::size_t removed = terminadventure::recent::PruneRecentFiles(state->recent_files))
    {
        std::cerr << "Removed " << removed << " stale recent entr"
                  << (removed == 1 ? "y" : "ies") << " from init.conf\n";
        terminadventure::config::WriteInit(init_path.string(), last_file, state->recent_files);
    }

    InputOption command_option;
    command_option.transform = [](InputState state)
    {
        state.element |= bgcolor(Color::Black) | color(Color::White);
        if (state.is_placeholder)
                state.element |= dim;
        return state.element;
    };
    command_option.cursor_position = &state->command_cursor;
    auto command_input = Input(&state->command_buffer, ":", command_option);

    auto command_wrapper = Renderer(command_input, [state, command_input] {
        if (state->mode != Mode::COMMAND)
            return emptyElement() | size(WIDTH, EQUAL, 0);
        return command_input->Render();
    });

    auto command_handler = CatchEvent(command_wrapper, [state](Event event) {
        if (event == Event::Tab)
        {
            terminadventure::op::CompleteCommand(state);
            return true;
        }
        if (event == Event::Escape || event == Event::Return) {
            if (event == Event::Return)
            {
                const std::string& buffer = state->command_buffer;
                if (!buffer.empty() && buffer[0] == '/')
                    terminadventure::op::ExecuteSearch(state, buffer);
                else
                    terminadventure::op::ExecuteCommand(state, buffer);
            }
            state->command_buffer.clear();
            state->command_cursor = 0;
            if (state->active_child)
                *state->active_child = 0;
            state->mode = state->mode_before_command;
            if (state->mode == Mode::TREE)
            {
                if (state->focus_treeview) state->focus_treeview();
            }
            else
            {
                if (state->focus_editor) state->focus_editor();
            }
            return true;
        }
        return false;
    });

    auto status_bar = Renderer([state]
    {
        std::string mode_str = ModeName(state->mode);
        if (state->changed)
        {
            mode_str += " [+]";
        }
        ftxui::Elements parts = {
            text(mode_str) | bold,
            // separator(),
            // text(" terminadventure ") | dim,
        };
        if (!state->status.empty())
        {
            parts.push_back(separator());
            parts.push_back(text(" " + state->status + " ") | dim);
        }
        return hbox(std::move(parts)) | bgcolor(Color::Blue);
    });

    int active_child = 0;
    state->active_child = &active_child;

    auto container = Container::Vertical({
        main_split | flex,
        command_handler,
        status_bar,
    }, &active_child);

    auto help_comp = terminadventure::help::MakeHelpDialog(state, config_path.string(), &show_help);
    auto search_comp = terminadventure::search::MakeSearchDialog(state, &show_search);
    auto link_picker_comp = terminadventure::search::MakeSearchDialog(state, &show_link_picker, true);
    auto bookmarks_comp = terminadventure::bookmarks::MakeBookmarksDialog(state, &show_bookmarks);
    auto links_comp = terminadventure::links::MakeLinksDialog(state, &show_links);
    auto dead_links_comp = terminadventure::brokenlinks::MakeDeadLinksDialog(state, &show_dead_links);
    auto history_comp = terminadventure::history::MakeHistoryDialog(state, &show_history);
    auto recent_comp = terminadventure::recent::MakeRecentDialog(state, &show_recent, recent_force);
    auto undo_comp = terminadventure::undo::MakeUndoDialog(state, &show_undo);
    auto browser_comp = terminadventure::browser::MakeFileBrowserDialog(state, &show_file_browser);
    auto modals = {
        std::tuple(help_comp,        &show_help),
        std::tuple(search_comp,      &show_search),
        std::tuple(link_picker_comp, &show_link_picker),
        std::tuple(bookmarks_comp,   &show_bookmarks),
        std::tuple(links_comp,       &show_links),
        std::tuple(dead_links_comp,  &show_dead_links),
        std::tuple(history_comp,     &show_history),
        std::tuple(recent_comp,      &show_recent),
        std::tuple(undo_comp,        &show_undo),
        std::tuple(browser_comp,     &show_file_browser),
    };

    auto root = container;
    for (auto& [comp, show] : modals)
    {
        root = Modal(root, comp, show);
    }

    screen.Loop(root);

    // Break the shared_ptr cycles the UI graph creates (the editor/treeview
    // components hold `state_`, while `state` holds them via focus_*/the op
    // lambdas capture `state`). Without this, EditorState and everything it
    // owns leak on return.
    state->operations.clear();
    state->focus_editor = nullptr;
    state->focus_treeview = nullptr;

    return 0;
}
