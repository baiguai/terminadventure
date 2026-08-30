#include "help.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "../editor/editor_state.hpp"

namespace terminadventure::help
{
    namespace
    {
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

        std::string PadRight(const std::string& s, std::size_t width)
        {
            if (s.size() >= width) return s;
            return s + std::string(width - s.size(), ' ');
        }

        std::string Lower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return s;
        }

        void ParseEntries(const std::string& path, std::vector<HelpEntry>& out)
        {
            std::ifstream file(path);
            if (!file.is_open()) return;

            std::string line;
            while (std::getline(file, line))
            {
                std::size_t hash = line.find_first_not_of(" \t");
                if (hash != std::string::npos && line[hash] == '#') line = line.substr(0, hash);

                auto fields = SplitFields(line);
                if (fields.size() < 6) continue;

                HelpEntry entry;
                entry.mode        = fields[0];
                entry.key         = fields[1];
                entry.command     = fields[3];
                entry.op          = fields[5];
                entry.description = (fields.size() > 6) ? fields[6] : "";

                // Entries without a key binding (GLOBAL) show their command
                // as the "key", e.g. ":qa", so the diagram and the ':'
                // key filter cover them too.
                if (entry.key == "-" && entry.command != "-")
                {
                    entry.key = ":" + entry.command;
                }

                // The diagram (and the filter) shows the binding and its
                // description; the op and command names stay hidden.
                entry.line = PadRight(entry.mode, 6) + " " +
                             PadRight(entry.key, 12) + " " +
                             entry.description;
                out.push_back(std::move(entry));
            }
        }
    }

    class HelpDialog : public ftxui::ComponentBase
    {
    public:
        HelpDialog(std::shared_ptr<EditorState> state, const std::string& path, bool* show)
            : state_(std::move(state)), show_(show)
        {
            ParseEntries(path, entries_);
            for (const auto& e : entries_)
            {
                content_width_ = std::max(content_width_, static_cast<int>(e.line.size()));
            }
        }

        bool Focusable() const override { return true; }

        bool OnEvent(ftxui::Event event) override
        {
            if (event == ftxui::Event::Escape)
            {
                *show_ = false;
                filter_.clear();
                scroll_ = 0;
                return true;
            }
            if (event == ftxui::Event::ArrowDown)
            {
                scroll_ = std::min(scroll_ + 1, MaxTop());
                return true;
            }
            if (event == ftxui::Event::ArrowUp)
            {
                scroll_ = std::max(0, scroll_ - 1);
                return true;
            }
            if (event == ftxui::Event::Backspace)
            {
                if (!filter_.empty())
                {
                    filter_.pop_back();
                    scroll_ = 0;
                }
                return true;
            }
            if (event.is_character())
            {
                filter_ += event.character();
                scroll_ = 0;
                return true;
            }
            return true;
        }

        ftxui::Element Render() override
        {
            const std::vector<int> filtered = Filtered();
            const int total = static_cast<int>(filtered.size());
            const int top = std::min(scroll_, MaxTop());
            const int count = std::min(kVisibleRows, std::max(0, total - top));

            ftxui::Elements rows;
            const int row_width = content_width_ + 2;
            if (total == 0)
            {
                rows.push_back(ftxui::text(PadRight(
                    filter_.empty() ? "  No bindings loaded (commands.conf missing?)"
                                    : "  No bindings match the filter",
                    row_width)) |
                               ftxui::dim);
            }
            else
            {
                for (int i = 0; i < count; ++i)
                {
                    ftxui::Element row = ftxui::text(" " + PadRight(entries_[filtered[top + i]].line, content_width_) + " ");
                    if (i == 0) row = row | ftxui::inverted;
                    rows.push_back(row);
                }
            }

            // Keep the dialog a fixed size even when few rows match: pad the
            // rows area out to kVisibleRows and every row out to a fixed
            // width, so the window never shrinks.
            while (static_cast<int>(rows.size()) < kVisibleRows)
            {
                rows.push_back(ftxui::text(PadRight("", row_width)));
            }

            const std::string footer =
                "  " + std::to_string(total == 0 ? 0 : top + 1) + "-" +
                std::to_string(top + count) + " of " + std::to_string(total) +
                "    Up/Down scroll  Esc close  ':key' matches keys only  ";

            return ftxui::window(ftxui::text(" ? Help "),
                                ftxui::vbox({
                                    ftxui::hbox({
                                        ftxui::text(" Filter: " + filter_ + "_"),
                                        ftxui::filler(),
                                    }),
                                    ftxui::separator(),
                                    ftxui::vbox(std::move(rows)),
                                    ftxui::separator(),
                                    ftxui::text(PadRight(footer, row_width)) | ftxui::dim,
                                })) |
                   ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 92) |
                   ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 24);
        }

    private:
        int MaxTop() const
        {
            return std::max(0, static_cast<int>(Filtered().size()) - kVisibleRows);
        }

        std::vector<int> Filtered() const
        {
            std::vector<int> result;

            std::string needle = Lower(filter_);
            bool key_only = false;
            if (!needle.empty() && needle[0] == ':')
            {
                key_only = true;
                needle = needle.substr(1);
            }

            for (std::size_t i = 0; i < entries_.size(); ++i)
            {
                const std::string& hay = key_only ? entries_[i].key : entries_[i].line;
                if (needle.empty() || Lower(hay).find(needle) != std::string::npos)
                {
                    result.push_back(static_cast<int>(i));
                }
            }
            return result;
        }

        std::shared_ptr<EditorState> state_;
        bool* show_;
        std::vector<HelpEntry> entries_;
        std::string filter_;
        int scroll_ = 0;
        int content_width_ = 72;
        static constexpr int kVisibleRows = 18;
    };

    ftxui::Component MakeHelpDialog(std::shared_ptr<EditorState> state,
                                    const std::string& config_path,
                                    bool* show)
    {
        return ftxui::Make<HelpDialog>(std::move(state), config_path, show);
    }
}
