#include "bookmarks.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "../editor/editor_state.hpp"
#include "../mode/mode.hpp"
#include "../treeview/tree_node.hpp"

namespace terminadventure::bookmarks
{
    namespace
    {
        std::string PadRight(const std::string& s, std::size_t width)
        {
            if (s.size() >= width) return s;
            return s + std::string(width - s.size(), ' ');
        }

        struct Entry
        {
            treeview::TreeNode* node = nullptr;
            int depth = 0;
            int line = -1;
        };
    }

    class BookmarksDialog : public ftxui::ComponentBase
    {
    public:
        BookmarksDialog(std::shared_ptr<EditorState> state, bool* show)
            : state_(std::move(state)), show_(show) {}

        bool Focusable() const override { return true; }

        bool pending_g_ = false;

        bool OnEvent(ftxui::Event event) override
        {
            if (event == ftxui::Event::Escape)
            {
                Close();
                return true;
            }
            if (event == ftxui::Event::Return)
            {
                Jump();
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
            if (event.is_character() && event.character() == "D")
            {
                Unbookmark();
                return true;
            }
            return true;
        }

        ftxui::Element Render() override
        {
            Recompute();

            const int total = static_cast<int>(entries_.size());
            const int sel = std::min(selection_, std::max(0, total - 1));

            const int max_top = std::max(0, total - kVisibleRows);
            int top = std::min(scroll_, max_top);
            if (sel < top) top = sel;
            if (sel >= top + kVisibleRows) top = sel - kVisibleRows + 1;
            top = std::max(0, std::min(top, max_top));
            const int count = std::min(kVisibleRows, std::max(0, total - top));

            ftxui::Elements rows;
            const int row_width = content_width_ + 2;
            if (total == 0)
            {
                rows.push_back(ftxui::text(PadRight("  No bookmarks", row_width)) | ftxui::dim);
            }
            else
            {
                for (int i = 0; i < count; ++i)
                {
                    ftxui::Element row = ftxui::text(" " + PadRight(rendered_[static_cast<std::size_t>(top + i)], content_width_) + " ");
                    if (top + i == sel) row = row | ftxui::inverted;
                    rows.push_back(row);
                }
            }
            while (static_cast<int>(rows.size()) < kVisibleRows)
            {
                rows.push_back(ftxui::text(PadRight("", row_width)));
            }

            const std::string footer =
                "  " + std::to_string(total == 0 ? 0 : sel + 1) + "/" + std::to_string(total) +
                "    j/k move  Enter jump  D unbookmark  Esc cancel  ";

            return ftxui::window(ftxui::text(" ` Bookmarks "),
                                ftxui::vbox({
                                    ftxui::separator(),
                                    ftxui::vbox(std::move(rows)),
                                    ftxui::separator(),
                                    ftxui::text(PadRight(footer, row_width)) | ftxui::dim,
                                })) |
                   ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 92) |
                   ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 24);
        }

    private:
        void Close()
        {
            *show_ = false;
            selection_ = 0;
            scroll_ = 0;
        }

        void Jump()
        {
            if (entries_.empty()) return;
            const int sel = std::min(selection_, static_cast<int>(entries_.size()) - 1);
            const Entry& entry = entries_[static_cast<std::size_t>(sel)];
            if (entry.node)
            {
                if (state_->reveal_node)
                {
                    state_->reveal_node(entry.node);
                }
                if (entry.line >= 0 && state_->reveal_line)
                {
                    state_->reveal_line(entry.line);
                    state_->mode = Mode::NORMAL;
                    if (state_->focus_editor) state_->focus_editor();
                }
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

        // Remove the selected bookmark. entries_ mirrors state_->bookmarks
        // one-for-one, so the selected row maps directly to a bookmark index.
        void Unbookmark()
        {
            if (entries_.empty()) return;
            const int sel = std::min(selection_, static_cast<int>(entries_.size()) - 1);
            state_->bookmarks.erase(state_->bookmarks.begin() + sel);
            selection_ = std::max(0, std::min(selection_,
                                              static_cast<int>(state_->bookmarks.size()) - 1));
            scroll_ = 0;
            state_->status = "Bookmark removed";
        }

        void Recompute()
        {
            std::map<std::string, Entry> by_id;
            if (state_->collect_all_nodes)
            {
                for (const auto& item : state_->collect_all_nodes())
                {
                    if (!item.first->id.empty())
                    {
                        by_id[item.first->id] = Entry{item.first, item.second, -1};
                    }
                }
            }

            entries_.clear();
            rendered_.clear();
            content_width_ = 20;
            for (const auto& mark : state_->bookmarks)
            {
                Entry entry;
                auto it = by_id.find(mark.id);
                if (it != by_id.end())
                {
                    entry = it->second;
                    entry.line = mark.line;
                }
                entries_.push_back(std::move(entry));

                std::string text;
                if (entry.node)
                {
                    text = std::string(static_cast<std::size_t>(entry.depth) * 2, ' ') + entry.node->name;
                    if (entry.line >= 0)
                    {
                        text += " (line " + std::to_string(entry.line + 1) + ")";
                    }
                }
                else
                {
                    text = "(deleted node)";
                }
                content_width_ = std::max(content_width_, static_cast<int>(text.size()));
                rendered_.push_back(std::move(text));
            }

            selection_ = std::max(0, std::min(selection_, static_cast<int>(entries_.size()) - 1));
        }

        std::shared_ptr<EditorState> state_;
        bool* show_;
        std::vector<Entry> entries_;
        std::vector<std::string> rendered_;
        int selection_ = 0;
        int scroll_ = 0;
        int content_width_ = 20;
        static constexpr int kVisibleRows = 18;
    };

    ftxui::Component MakeBookmarksDialog(std::shared_ptr<EditorState> state, bool* show)
    {
        return ftxui::Make<BookmarksDialog>(std::move(state), show);
    }
}
