#include "config.hpp"

#include <fstream>
#include <string>
#include <vector>

#include <ftxui/component/event.hpp>

#include "../editor/editor_state.hpp"
#include "../op/op.hpp"

namespace terminadventure::config
{
    namespace
    {
        using ftxui::Event;

        std::vector<std::string> SplitFields(const std::string& line)
        {
            std::vector<std::string> fields;
            std::size_t i = 0;
            while (i < line.size())
            {
                while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;

                if (i >= line.size()) break;

                if (line[i] == '"')
                {
                    ++i;
                    std::string token;
                    while (i < line.size() && line[i] != '"') token += line[i++];
                    ++i;
                    fields.push_back(token);
                }
                else
                {
                    std::string token;
                    while (i < line.size() && line[i] != ' ' && line[i] != '\t') token += line[i++];

                    fields.push_back(token);
                }
            }
            return fields;
        }

        bool ParseKey(const std::string& token, Event& out)
        {
            if (token == "Esc")         { out = Event::Escape;    return true; }
            if (token == "Return")      { out = Event::Return;    return true; }
            if (token == "Tab")         { out = Event::Tab;       return true; }
            if (token == "Backspace")   { out = Event::Backspace; return true; }
            if (token == "Space")       { out = Event::Character(' '); return true; }
            if (token == "ArrowUp")     { out = Event::ArrowUp;   return true; }
            if (token == "ArrowDown")   { out = Event::ArrowDown; return true; }
            if (token == "ArrowLeft")   { out = Event::ArrowLeft; return true; }
            if (token == "ArrowRight")  { out = Event::ArrowRight; return true; }
            if (token == "PageUp")      { out = Event::PageUp;    return true; }
            if (token == "PageDown")    { out = Event::PageDown;  return true; }
            if (token == "Home")        { out = Event::Home;      return true; }
            if (token == "End")         { out = Event::End;       return true; }
            if (token == "CtrlV")       { out = Event::Special("\x16"); return true; }
            if (token == "CtrlC")       { out = Event::Special("\x03"); return true; }
            if (token == "CtrlX")       { out = Event::Special("\x18"); return true; }
            if (token == "CtrlDash")    { out = Event::Special("\x1f"); return true; }
            if (token == "CtrlQuote")   { out = Event::Special("\x07"); return true; }
            if (token == "CtrlRBracket"){ out = Event::Special("\x1d"); return true; }
            if (token.size() == 1)      { out = Event::Character(token[0]); return true; }
            return false;
        }

        Keymap& ModeKeymap(std::shared_ptr<EditorState> state, const std::string& mode)
        {
            if (mode == "TREE")          return state->tree_keymap;
            if (mode == "NORMAL")        return state->normal_keymap;
            if (mode == "INSERT")        return state->insert_keymap;
            if (mode == "VISUAL_BLOCK")  return state->visual_block_keymap;
            return state->visual_keymap; // VISUAL, VISUAL_LINE
        }
    }

    bool LoadConfig(const std::string& path, std::shared_ptr<EditorState> state)
    {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        std::string line;
        while (std::getline(file, line))
        {
            std::size_t hash = line.find_first_not_of(" \t");
            if (hash != std::string::npos && line[hash] == '#') line = line.substr(0, hash);
            auto fields = SplitFields(line);
            if (fields.empty()) continue;
            if (fields.size() < 6) return false;

            const std::string& mode     = fields[0];
            const std::string& key      = fields[1];
            const std::string& repeat   = fields[2];
            const std::string& command  = fields[3];
            const std::string& args     = fields[4];
            const std::string& op       = fields[5];

            if (state->operations.find(op) == state->operations.end()) return false;

            bool can_repeat = (repeat == "yes");

            if (command != "-") state->commands[command] = op;

            if (mode == "GLOBAL")
            {
                continue;
            }

            Keymap& km = ModeKeymap(state, mode);
            if (can_repeat) km.EnableCounts();

            auto key_parts = SplitFields(key);
            std::vector<Event> events;
            for (const auto& t : key_parts)
            {
                Event e;
                if (!ParseKey(t, e)) return false;
                events.push_back(e);
            }

            Binding binding{op, command, args, can_repeat};
            if (events.size() == 1) km.Bind(events[0], std::move(binding));
            else km.Bind(events, std::move(binding));
        }
        return true;
    }

    bool ReadInit(const std::string& path, std::string& last_file, std::vector<std::string>& recent_files)
    {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        last_file.clear();
        recent_files.clear();
        std::string line;
        while (std::getline(file, line))
        {
            std::size_t start = line.find_first_not_of(" \t");
            if (start == std::string::npos) continue;
            if (line[start] == '#') continue;

            std::size_t eq = line.find('=', start);
            if (eq == std::string::npos) continue;

            std::size_t kend = line.find_last_not_of(" \t", eq - 1);
            std::string key = (kend == std::string::npos)
                                  ? ""
                                  : line.substr(start, kend - start + 1);

            std::size_t p = line.find_first_not_of(" \t", eq + 1);
            std::string value;
            if (p != std::string::npos)
            {
                std::size_t end = line.find_last_not_of(" \t");
                value = line.substr(p, end - p + 1);
            }

            if (key == "last_file")
            {
                last_file = value;
            }
            else if (key == "recent_file")
            {
                if (!value.empty())
                {
                    recent_files.push_back(value);
                }
            }
        }
        return true;
    }

    bool WriteInit(const std::string& path, const std::string& last_file, const std::vector<std::string>& recent_files)
    {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        file << "# terminadventure init configuration - written by the app\n";
        file << "# On startup the app reopens `last_file` when it still exists.\n";
        file << "last_file = " << last_file << "\n";
        for (const auto& recent : recent_files)
        {
            file << "recent_file = " << recent << "\n";
        }
        return static_cast<bool>(file);
    }

    bool WriteRecentFiles(const std::string& path, const std::vector<std::string>& recent_files)
    {
        std::string last_file;
        std::vector<std::string> ignored;
        ReadInit(path, last_file, ignored);
        return WriteInit(path, last_file, recent_files);
    }
}
