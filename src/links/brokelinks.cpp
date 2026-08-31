#include "links.hpp"
#include "brokelinks.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include "../editor/editor_state.hpp"
#include "../treeview/tree_node.hpp"

namespace terminadventure::brokenlinks
{
    namespace
    {
        std::string PadRight(const std::string& s, std::size_t width)
        {
            if (s.size() >= width) return s;
            return s + std::string(width - s.size(), ' ');
        }

        // 0-based line of the first real occurrence of `target` (a `_Title_`
        // reference) in `text`. A candidate counts only when its neighbours
        // are not word characters, mirroring the `_Note_` scanning rules.
        int FirstOccurrenceLine(const std::string& text, const std::string& target)
        {
            auto is_word = [](char c) {
                return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                       || (c >= '0' && c <= '9');
            };
            std::size_t pos = text.find(target);
            while (pos != std::string::npos)
            {
                const std::size_t after = pos + target.size();
                const bool before_ok = pos == 0 || !is_word(text[pos - 1]);
                const bool after_ok = after == text.size() || !is_word(text[after]);
                if (before_ok && after_ok)
                {
                    int line = 0;
                    for (std::size_t i = 0; i < pos; ++i)
                    {
                        if (text[i] == '\n') ++line;
                    }
                    return line;
                }
                pos = text.find(target, pos + 1);
            }
            return 0;
        }
    }

    class DeadLinksDialog : public ftxui::ComponentBase
    {
    public:
        DeadLinksDialog(std::shared_ptr<EditorState> state, bool* show)
            : state_(std::move(state)), show_(show) {}

        bool Focusable() const override { return true; }

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
                rows.push_back(ftxui::text(PadRight("  No dead links found", row_width)) | ftxui::dim);
            }
            else
            {
                for (int i = 0; i < count; ++i)
                {
                    const int index = top + i;
                    ftxui::Element row =
                        ftxui::text(" " + PadRight(rendered_[static_cast<std::size_t>(index)], content_width_) + " ");
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
                "    j/k move  Enter or double-click jump to first occurrence  Esc cancel  ";

            return ftxui::window(ftxui::text(" # Dead Links "),
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
        struct Entry
        {
            std::string target;
            treeview::TreeNode* node = nullptr;
            int line = 0;
        };

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
            const Entry& entry = entries_[static_cast<std::size_t>(sel)];
            if (state_->reveal_node)
            {
                state_->reveal_node(entry.node);
            }
            if (state_->reveal_line)
            {
                state_->reveal_line(entry.line);
            }
            Close();
        }

        void MoveSelection(int dir)
        {
            if (entries_.empty()) return;
            const int total = static_cast<int>(entries_.size());
            selection_ = std::max(0, std::min(total - 1, selection_ + dir));
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

            // Titles of existing notes, lowercased and trimmed, for resolving
            // `_Title_` references (case-insensitive, as in the links dialog).
            std::map<std::string, treeview::TreeNode*> by_title;
            if (state_->collect_all_nodes)
            {
                for (const auto& item : state_->collect_all_nodes())
                {
                    const std::string key =
                        terminadventure::links::Lower(terminadventure::links::Trim(item.first->name));
                    if (!key.empty())
                    {
                        by_title.emplace(key, item.first);
                    }
                }
            }

            // Scan every node in document order; each broken title is listed
            // once, pointing at its first occurrence anywhere in the document.
            std::set<std::string> listed;
            if (state_->collect_all_nodes)
            {
                for (const auto& item : state_->collect_all_nodes())
                {
                    treeview::TreeNode* node = item.first;
                    std::vector<terminadventure::links::Link> links;
                    terminadventure::links::CollectLinks(node->text, links);
                    for (const auto& link : links)
                    {
                        if (!terminadventure::links::IsNoteTarget(link.target)) continue;
                        const std::string title_key =
                            terminadventure::links::Lower(terminadventure::links::NoteTitle(link.target));
                        if (by_title.count(title_key) != 0) continue;  // resolves
                        if (listed.count(title_key) != 0) continue;    // already listed
                        listed.insert(title_key);
                        entries_.push_back(Entry{link.target, node,
                                                 FirstOccurrenceLine(node->text, link.target)});
                    }
                }
            }

            for (const auto& e : entries_)
            {
                const std::string text = terminadventure::links::NoteTitle(e.target)
                                         + "   (in " + e.node->name + ")";
                content_width_ = std::max(content_width_, static_cast<int>(text.size()));
                rendered_.push_back(text);
            }

            selection_ = std::max(0, std::min(selection_, static_cast<int>(entries_.size()) - 1));
        }

        std::shared_ptr<EditorState> state_;
        bool* show_;
        ftxui::Box box_;
        std::vector<Entry> entries_;
        std::vector<std::string> rendered_;
        int selection_ = 0;
        int scroll_ = 0;
        int content_width_ = 20;
        int last_click_index_ = -1;
        std::chrono::steady_clock::time_point last_click_time_;
    };

    ftxui::Component MakeDeadLinksDialog(std::shared_ptr<EditorState> state, bool* show)
    {
        return ftxui::Make<DeadLinksDialog>(std::move(state), show);
    }
}
