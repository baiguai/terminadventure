#include "op.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "../editor/editor_state.hpp"
#include "../mode/mode.hpp"
#include "../search/search.hpp"

namespace terminadventure::op
{
    namespace
    {
        bool IsPathCommand(const std::string& name)
        {
            return name == "open" || name == "o" || name == "o!"
                || name == "saveas"
                || name == "U" || name == "X" || name == "x"
                || name == "export_note_txt" || name == "export_tree_txt";
        }

        void CollectPathMatches(const std::string& arg, std::vector<std::string>& matches)
        {
            namespace fs = std::filesystem;

            // Decompose the argument into (directory, filename prefix) using
            // std::filesystem so each platform handles its own conventions:
            // '/' and '\' separators, drive letters (C:\), trailing slashes.
            const fs::path p(arg);
            const std::string prefix = p.filename().string();
            const std::string parent = p.parent_path().string();

            // Join candidates with the separator the user is typing. A '/'
            // anywhere wins even on Windows, where both separators are valid.
            char sep = fs::path::preferred_separator;
            if (parent.find('/') != std::string::npos) sep = '/';

            const std::string search = parent.empty() ? "." : parent;

            std::error_code ec;
            for (auto it = fs::directory_iterator(search, ec), end = fs::directory_iterator();
                 it != end && !ec; it.increment(ec))
            {
                std::string entry = it->path().filename().string();
                if (entry.empty()) continue;
                if (entry[0] == '.')
                {
                    if (prefix.empty() || prefix[0] != '.') continue;
                }
                if (!prefix.empty() && entry.compare(0, prefix.size(), prefix) != 0) continue;

                std::error_code ec2;
                bool is_dir = it->is_directory(ec2);
                std::string full;
                if (parent.empty())
                {
                    full = entry;
                }
                else if (parent.back() == '/' || parent.back() == '\\')
                {
                    full = parent + entry;   // parent is already a root ("/", "C:\")
                }
                else
                {
                    full = parent + sep + entry;
                }
                if (is_dir) full += sep;
                matches.push_back(std::move(full));
            }
            std::sort(matches.begin(), matches.end());
        }

        std::string CommonPrefix(const std::vector<std::string>& items)
        {
            if (items.empty()) return "";
            std::string common = items[0];
            for (std::size_t i = 1; i < items.size(); ++i)
            {
                std::size_t j = 0;
                while (j < common.size() && j < items[i].size() && common[j] == items[i][j]) ++j;
                common = common.substr(0, j);
            }
            return common;
        }

        std::string Join(const std::vector<std::string>& items)
        {
            std::string out;
            for (std::size_t i = 0; i < items.size(); ++i)
            {
                if (i != 0) out += ' ';
                out += items[i];
            }
            return out;
        }
    }

    void CompleteCommand(std::shared_ptr<EditorState> state)
    {
        std::string cmd = state->command_buffer;
        if (!cmd.empty() && cmd[0] == '/') return;  // searching, not a command
        if (!cmd.empty() && cmd[0] == ':') cmd.erase(0, 1);

        std::size_t space = cmd.find(' ');
        if (space == std::string::npos)
        {
            std::vector<std::string> matches;
            for (const auto& [name, op] : state->commands)
            {
                if (name.compare(0, cmd.size(), cmd) == 0) matches.push_back(name);
            }
            if (matches.empty()) return;
            std::sort(matches.begin(), matches.end());

            std::string completed;
            if (std::find(matches.begin(), matches.end(), cmd) != matches.end())
            {
                completed = cmd + " ";
            }
            else
            {
                std::string common = CommonPrefix(matches);
                completed = (common == cmd) ? cmd : (common + " ");
            }
            state->command_buffer = ":" + completed;
            state->command_cursor = static_cast<int>(state->command_buffer.size());
            if (matches.size() > 1) state->status = "Matches: " + Join(matches);
            return;
        }

        std::string name = cmd.substr(0, space);
        if (!IsPathCommand(name)) return;

        std::string arg = cmd.substr(space + 1);
        std::vector<std::string> matches;
        CollectPathMatches(arg, matches);
        if (matches.empty())
        {
            state->status = "No match for: " + arg;
            return;
        }

        std::string completed;
        if (matches.size() == 1)
        {
            completed = matches[0];
        }
        else
        {
            completed = CommonPrefix(matches);
            state->status = "Matches: " + Join(matches);
        }
        if (completed == arg) return;

        state->command_buffer = ":" + name + " " + completed;
        state->command_cursor = static_cast<int>(state->command_buffer.size());
    }

    void OpenCommandLine(std::shared_ptr<EditorState> state, const std::string& command)
    {
        state->mode_before_command = state->mode;
        state->mode = Mode::COMMAND;
        state->command_buffer = command.empty() ? ":" : (":" + command + " ");
        state->command_cursor = static_cast<int>(state->command_buffer.size());
        if (state->active_child) *state->active_child = 1;
    }

    void OpenSearchCommand(std::shared_ptr<EditorState> state)
    {
        state->mode_before_command = state->mode;
        state->mode = Mode::COMMAND;
        state->command_buffer = "/";
        state->command_cursor = static_cast<int>(state->command_buffer.size());
        if (state->active_child) *state->active_child = 1;
    }

    void OpenCommandLineWithArgs(std::shared_ptr<EditorState> state,
                                 const std::string& command, const std::string& args)
    {
        state->mode_before_command = state->mode;
        state->mode = Mode::COMMAND;
        state->command_buffer = ":" + command + (args.empty() ? "" : " " + args);
        state->command_cursor = static_cast<int>(state->command_buffer.size());
        if (state->active_child) *state->active_child = 1;
    }

    bool ExecuteCommand(std::shared_ptr<EditorState> state, const std::string& input)
    {
        std::string cmd = input;
        if (!cmd.empty() && cmd[0] == ':') cmd = cmd.substr(1);

        std::size_t start = cmd.find_first_not_of(" \t");
        if (start == std::string::npos) return false;

        cmd = cmd.substr(start);
        std::size_t space = cmd.find_first_of(" \t");
        std::string name = cmd.substr(0, space);
        std::string args = (space == std::string::npos) ? "" : cmd.substr(space + 1);

        auto cmd_it = state->commands.find(name);
        if (cmd_it == state->commands.end()) return false;
        auto op_it = state->operations.find(cmd_it->second);
        if (op_it == state->operations.end()) return false;
        op_it->second(args, 1);
        return true;
    }

    bool ExecuteSearch(std::shared_ptr<EditorState> state, const std::string& input)
    {
        if (input.empty() || input[0] != '/') return false;

        // An empty query re-runs the previous search, as in Vim.
        std::string raw = input.substr(1);
        if (raw.empty())
        {
            if (state->search_query.empty())
            {
                state->status = "No previous search";
                return true;
            }
            raw = state->search_query;
        }

        state->search_query = raw;
        state->search_matches = terminadventure::search::FindMatches(state, raw);
        state->search_index = -1;

        if (state->search_matches.empty())
        {
            state->search_active = true;
            state->status = "Pattern not found: " + raw;
            return true;
        }

        state->search_active = true;
        state->search_index = 0;
        state->search_reveal_pending = true;
        if (state->reveal_node) state->reveal_node(state->search_matches[0]);
        state->status = "Match 1 of " + std::to_string(state->search_matches.size());
        return true;
    }

    Keymap::Result Resolve(std::shared_ptr<EditorState> state, ftxui::Event event)
    {
        return state->ActiveKeymap().Handle(event);
    }

    bool Dispatch(std::shared_ptr<EditorState> state, const Keymap::Result& result)
    {
        if (result.pending)
            return true;
        if (result.op.empty())
            return false;

        if (result.args == "prompt" && !result.command.empty() && result.command != "-")
        {
            OpenCommandLine(state, result.command);
            return true;
        }

        auto it = state->operations.find(result.op);
        if (it == state->operations.end())
            return false;
        it->second((result.args == "-") ? "" : result.args, result.count);
        return true;
    }

    bool HandleKey(std::shared_ptr<EditorState> state, ftxui::Event event)
    {
        // Some terminals deliver Enter as an ESC sequence instead of CR/LF:
        // SS3 "ESC O M" (application-keypad Enter) and CSI "ESC [ 1 9 ~".
        // Normalize them so Return bindings fire in every mode.
        if (event == ftxui::Event::Special("\x1bOM")
            || event == ftxui::Event::Special("\x1b[19~"))
        {
            event = ftxui::Event::Return;
        }
        return Dispatch(state, Resolve(state, event));
    }
}
