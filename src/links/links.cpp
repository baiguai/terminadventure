#include "links.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <map>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include "../clipboard/clipboard.hpp"
#include "../editor/editor_state.hpp"
#include "../treeview/tree_node.hpp"

namespace terminadventure::links
{
    std::string PadRight(const std::string& s, std::size_t width)
    {
        if (s.size() >= width) return s;
        return s + std::string(width - s.size(), ' ');
    }

    std::string Trim(const std::string& s)
    {
        std::size_t first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        std::size_t last = s.find_last_not_of(" \t\r\n");
        return s.substr(first, last - first + 1);
    }

    std::string Lower(const std::string& s)
    {
        std::string out = s;
        for (char& c : out)
        {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        }
        return out;
    }

    bool IsNoteTarget(const std::string& target)
    {
        return target.size() >= 3 && target.front() == '_' && target.back() == '_';
    }

    bool IsUrlTarget(const std::string& target)
    {
        return target.size() >= 7 &&
               (target.rfind("http://", 0) == 0 ||
                target.rfind("https://", 0) == 0 ||
                target.rfind("file://", 0) == 0);
    }

    std::string NoteTitle(const std::string& target)
    {
        return target.substr(1, target.size() - 2);
    }

    // Mirrors the three regexes of the HTML app's openLinksDialog().
    const std::regex kMarkdownRegex(
        R"(\[([^\]]+)\](?:[ \t]*\r?\n[ \t]*|[ \t]*)(https?://\S+|file://\S+|_[^_]+_))",
        std::regex_constants::icase);
    const std::regex kUrlRegex(R"(\b(?:https?|file)://[^\s]+)",
                               std::regex_constants::icase);
    const std::regex kNoteRegex(
        R"((?:^|\s|\()_([^_]|[^_].*?[^_])_(?=\s|[.,!?;:)]|$))");

    void CollectLinks(const std::string& content, std::vector<Link>& out)
    {
        // Markdown-style: [Label] optionally followed by whitespace/newline
        // then a URL or a _Note_ reference.
        {
            std::smatch match;
            std::string::const_iterator begin = content.begin();
            std::string::const_iterator end = content.end();
            while (std::regex_search(begin, end, match, kMarkdownRegex))
            {
                out.push_back(Link{Trim(match[1].str()), Trim(match[2].str()),
                                   nullptr, true, true});
                begin = match[0].second;
            }
        }
        // Standalone URLs not already captured.
        {
            std::smatch match;
            std::string::const_iterator begin = content.begin();
            std::string::const_iterator end = content.end();
            while (std::regex_search(begin, end, match, kUrlRegex))
            {
                const std::string target = match[0].str();
                const bool seen = std::any_of(
                    out.begin(), out.end(),
                    [&target](const Link& l) { return l.target == target; });
                if (!seen)
                {
                    out.push_back(Link{target, target, nullptr, false, true});
                }
                begin = match[0].second;
            }
        }
        // _Note_ references not already captured.
        {
            std::smatch match;
            std::string::const_iterator begin = content.begin();
            std::string::const_iterator end = content.end();
            while (std::regex_search(begin, end, match, kNoteRegex))
            {
                const std::string title = Trim(match[1].str());
                const std::string wrapped = "_" + title + "_";
                const bool seen = std::any_of(
                    out.begin(), out.end(),
                    [&wrapped](const Link& l) { return l.target == wrapped; });
                if (!seen)
                {
                    out.push_back(Link{title, wrapped, nullptr, false, true});
                }
                begin = match[0].second;
            }
        }
    }

    namespace
    {
        void OpenExternal(const std::string& url)
        {
            std::string escaped;
            for (const char c : url)
            {
                if (c == '"' || c == '\\' || c == '$' || c == '`') escaped += '\\';
                escaped += c;
            }
#if defined(_WIN32)
            const std::string command = "start \"\" \"" + escaped + "\"";
#else
            const std::string command =
                "nohup xdg-open \"" + escaped + "\" >/dev/null 2>&1 &";
#endif
            const int rc = std::system(command.c_str());
            (void)rc;
        }

        bool CopyToClipboard(const std::string& text)
        {
            return terminadventure::clipboard::Write(text);
        }
    }

    class LinksDialog : public ftxui::ComponentBase
    {
    public:
        LinksDialog(std::shared_ptr<EditorState> state, bool* show)
            : state_(std::move(state)), show_(show) {}

        bool Focusable() const override { return true; }

        bool pending_g_ = false;

        bool OnEvent(ftxui::Event event) override
        {
            if (event.is_mouse())
            {
                return OnMouse(event.mouse());
            }
            if (event == ftxui::Event::Escape)
            {
                Close();
                return true;
            }
            if (event == ftxui::Event::Return)
            {
                Activate();
                return true;
            }
            if (event.is_character() && event.character() == "g")
            {
                if (pending_g_)
                {
                    pending_g_ = false;
                    MoveToStart();
                    return true;
                }
                pending_g_ = true;
                return true;
            }
            if ((event.is_character() && event.character() == "G"))
            {
                pending_g_ = false;
                MoveToEnd();
                return true;
            }

            pending_g_ = false;

            if (event.is_character() && event.character() == "y")
            {
                CopySelected();
                return true;
            }
            if (event == ftxui::Event::ArrowDown
                || (event.is_character() && event.character() == "j"))
            {
                MoveSelection(+1);
                return true;
            }

            if (event == ftxui::Event::ArrowUp
                || (event.is_character() && event.character() == "k"))
            {
                MoveSelection(-1);
                return true;
            }

            return true;
        }

        ftxui::Element Render() override
        {
            Recompute();

            const int total = static_cast<int>(entries_.size());
            const int sel = std::min(selection_, std::max(0, total - 1));
            const int top = ComputeTop(sel, total);
            const int count = std::min(kVisibleRows, std::max(0, total - top));

            ftxui::Elements rows;
            const int row_width = content_width_ + 2;
            if (total == 0)
            {
                rows.push_back(ftxui::text(PadRight("  No links found", row_width)) | ftxui::dim);
            }
            else
            {
                for (int i = 0; i < count; ++i)
                {
                    const int index = top + i;
                    ftxui::Element row =
                        ftxui::text(" " + PadRight(rendered_[static_cast<std::size_t>(index)], content_width_) + " ");
                    if (!entries_[static_cast<std::size_t>(index)].ok)
                    {
                        row = row | ftxui::dim;
                    }
                    if (index == sel) row = row | ftxui::inverted;
                    rows.push_back(row);
                }
            }
            while (static_cast<int>(rows.size()) < kVisibleRows)
            {
                rows.push_back(ftxui::text(PadRight("", row_width)));
            }

            const std::string footer =
                "  " + std::to_string(total == 0 ? 0 : sel + 1) + "/" + std::to_string(total) +
                "    j/k move  Enter or double-click open  y copy  Esc cancel  ";

            return ftxui::window(ftxui::text(" # Links "),
                                 ftxui::vbox({
                                     ftxui::separator(),
                                     ftxui::vbox(std::move(rows)),
                                     ftxui::separator(),
                                     ftxui::text(PadRight(footer, row_width)) | ftxui::dim,
                                 })) |
                   ftxui::reflect(box_) |
                   ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 92) |
                   ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 24);
        }

    private:
        static constexpr int kVisibleRows = 18;
        static constexpr int kDoubleClickMs = 500;

        bool OnMouse(ftxui::Mouse mouse)
        {
            const auto now = std::chrono::steady_clock::now();
            if (mouse.button == ftxui::Mouse::Left
                && mouse.motion == ftxui::Mouse::Pressed)
            {
                if (!box_.Contain(mouse.x, mouse.y))
                {
                    // Clicking outside the dialog dismisses it.
                    Close();
                    return true;
                }
                const int total = static_cast<int>(entries_.size());
                if (total > 0)
                {
                    const int sel = std::min(selection_, total - 1);
                    const int top = ComputeTop(sel, total);
                    const int index = top + (mouse.y - (box_.y_min + 2));
                    if (index >= 0 && index < total)
                    {
                        const bool dbl =
                            index == last_click_index_
                            && now - last_click_time_
                                   <= std::chrono::milliseconds(kDoubleClickMs);
                        selection_ = index;
                        last_click_index_ = index;
                        last_click_time_ = now;
                        if (dbl)
                        {
                            Activate();
                        }
                    }
                }
            }
            return true;
        }

        void Close()
        {
            *show_ = false;
            selection_ = 0;
            scroll_ = 0;
            last_click_index_ = -1;
        }

        void Activate()
        {
            if (entries_.empty()) return;
            const int sel = std::min(selection_, static_cast<int>(entries_.size()) - 1);
            const Link& link = entries_[static_cast<std::size_t>(sel)];
            if (!link.ok)
            {
                state_->status = IsNoteTarget(link.target)
                                     ? "Node not found: " + NoteTitle(link.target)
                                     : "Unrecognized link format";
                return;
            }
            if (IsUrlTarget(link.target))
            {
                OpenExternal(link.target);
                return;
            }
            if (link.node && state_->reveal_node)
            {
                state_->reveal_node(link.node);
            }
            state_->status = "";
            Close();
        }

        void MoveSelection(int dir)
        {
            if (entries_.empty()) return;
            const int total = static_cast<int>(entries_.size());
            selection_ = std::max(0, std::min(total - 1, selection_ + dir));
        }

        void MoveToStart()
        {
            selection_ = 0;
        }

        void MoveToEnd()
        {
            if (!entries_.empty())
            {
                selection_ = static_cast<int>(entries_.size()) -1;
            }
        }

        // Mirror of the HTML app's `y` key: copy a URL verbatim, or a note
        // link as its `_Title_` reference. Missing notes copy nothing.
        void CopySelected()
        {
            if (entries_.empty()) return;
            const int sel = std::min(selection_, static_cast<int>(entries_.size()) - 1);
            const Link& link = entries_[static_cast<std::size_t>(sel)];

            std::string text;
            if (link.node)
            {
                text = "_" + NoteTitle(link.target) + "_";
            }
            else if (IsUrlTarget(link.target))
            {
                text = link.target;
            }
            if (text.empty()) return;

            if (CopyToClipboard(text))
            {
                state_->status = "Copied: " + text;
            }
            else
            {
                state_->status = "Clipboard unavailable";
            }
        }

        int ComputeTop(int sel, int total) const
        {
            const int max_top = std::max(0, total - kVisibleRows);
            int top = std::min(scroll_, max_top);
            if (sel < top) top = sel;
            if (sel >= top + kVisibleRows) top = sel - kVisibleRows + 1;
            return std::max(0, std::min(top, max_top));
        }

        void Recompute()
        {
            entries_.clear();
            rendered_.clear();
            content_width_ = 20;

            if (state_->active_node)
            {
                CollectLinks(state_->active_node->text, entries_);
            }

            std::map<std::string, treeview::TreeNode*> by_title;
            if (state_->collect_all_nodes)
            {
                for (const auto& item : state_->collect_all_nodes())
                {
                    const std::string key = Lower(Trim(item.first->name));
                    if (!key.empty())
                    {
                        by_title.emplace(key, item.first);
                    }
                }
            }

            for (auto& link : entries_)
            {
                if (IsUrlTarget(link.target))
                {
                    link.ok = true;
                }
                else if (IsNoteTarget(link.target))
                {
                    const auto it = by_title.find(Lower(NoteTitle(link.target)));
                    link.node = (it != by_title.end()) ? it->second : nullptr;
                    link.ok = link.node != nullptr;
                }
                else
                {
                    link.ok = false;
                }

                std::string text = link.from_markdown ? link.label : link.target;
                if (text.size() >= 2 && text.front() == '_' && text.back() == '_')
                {
                    text = text.substr(1, text.size() - 2);
                }
                if (link.ok && link.node)
                {
                    text += "  ->  " + link.node->name;
                }
                content_width_ = std::max(content_width_, static_cast<int>(text.size()));
                rendered_.push_back(std::move(text));
            }

            selection_ = std::max(0, std::min(selection_, static_cast<int>(entries_.size()) - 1));
        }

        std::shared_ptr<EditorState> state_;
        bool* show_;
        ftxui::Box box_;
        std::vector<Link> entries_;
        std::vector<std::string> rendered_;
        int selection_ = 0;
        int scroll_ = 0;
        int content_width_ = 20;
        int last_click_index_ = -1;
        std::chrono::steady_clock::time_point last_click_time_;
    };

    ftxui::Component MakeLinksDialog(std::shared_ptr<EditorState> state, bool* show)
    {
        return ftxui::Make<LinksDialog>(std::move(state), show);
    }
}
