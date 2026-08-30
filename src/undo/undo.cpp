#include "undo.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "../bookmark/bookmark.hpp"
#include "../io/serialize.hpp"

namespace terminadventure::undo
{
    namespace
    {
        std::string Trunc(std::string s, std::size_t width)
        {
            if (s.size() > width)
            {
                s.resize(width);
            }
            return s;
        }

        // Diff markers for the preview pane: 0 = unchanged, 1 = changed,
        // 2 = first changed line.
        std::vector<int> DiffMark(const std::vector<std::string>& snap,
                                  const std::vector<std::string>& cur)
        {
            const std::size_t n = std::max(snap.size(), cur.size());
            std::vector<int> mark(n, 0);
            bool first = true;
            for (std::size_t i = 0; i < n; ++i)
            {
                const std::string& a = (i < snap.size()) ? snap[i] : "";
                const std::string& b = (i < cur.size()) ? cur[i] : "";
                if (a != b)
                {
                    mark[i] = first ? 2 : 1;
                    first = false;
                }
            }
            return mark;
        }
    }

    namespace
    {
        std::vector<std::string> SplitLines(const std::string& text)
        {
            std::vector<std::string> out;
            std::size_t start = 0;
            while (start <= text.size())
            {
                const std::size_t nl = text.find('\n', start);
                if (nl == std::string::npos)
                {
                    out.push_back(text.substr(start));
                    break;
                }
                out.push_back(text.substr(start, nl - start));
                start = nl + 1;
            }
            return out;
        }

        treeview::TreeNode* FindNode(std::vector<treeview::TreeNode>& nodes,
                                     const std::string& id)
        {
            for (auto& node : nodes)
            {
                if (node.id == id) return &node;
                if (treeview::TreeNode* found = FindNode(node.children, id)) return found;
            }
            return nullptr;
        }

        // Text lines of the node `id` inside a serialized snapshot. The undo
        // dialog shows only the currently active note, so this pulls that one
        // node's text out of the snapshot rather than rendering the whole
        // document.
        std::vector<std::string> NodeTextDoc(const std::string& json,
                                             const std::string& id)
        {
            std::vector<treeview::TreeNode> roots;
            int width = 0;
            std::vector<bookmark::Bookmark> marks;
            std::vector<std::string> hist;
            if (!io::Deserialize(json, roots, &width, &marks, &hist)) return {};
            treeview::TreeNode* node = FindNode(roots, id);
            if (node == nullptr) return {};
            return SplitLines(node->text);
        }
    }

    std::string FirstTextLine(const treeview::TreeNode& node)
    {
        const std::size_t nl = node.text.find('\n');
        return (nl == std::string::npos) ? node.text : node.text.substr(0, nl);
    }

    class UndoDialog : public ftxui::ComponentBase
    {
    public:
        UndoDialog(std::shared_ptr<EditorState> state, bool* show)
            : state_(std::move(state)), show_(show) {}

        bool Focusable() const override { return true; }

        bool OnEvent(ftxui::Event event) override
        {
            if (event == ftxui::Event::Escape)
            {
                Close();
                return true;
            }
            if (event == ftxui::Event::Return)
            {
                Apply();
                return true;
            }
            if (event == ftxui::Event::ArrowDown
                    || (event.is_character() && event.character() == "j"))
            {
                Move(+1);
                return true;
            }
            if (event == ftxui::Event::ArrowUp
                    || (event.is_character() && event.character() == "k"))
            {
                Move(-1);
                return true;
            }
            if (event.is_character() && event.character() == "g")
            {
                selection_ = 0;
                return true;
            }
            if (event.is_character() && event.character() == "G")
            {
                selection_ = static_cast<int>(state_->undo_stack.size()) - 1;
                return true;
            }
            return true; // consume everything else
        }

        ftxui::Element Render() override
        {
            const auto& stack = state_->undo_stack;
            const int total = static_cast<int>(stack.size());
            const int sel = std::min(selection_, std::max(0, total - 1));

            const std::string active_id = state_->active_node ? state_->active_node->id : "";

            std::vector<std::string> cur;
            if (state_->active_node)
            {
                cur = SplitLines(state_->active_node->text);
            }

            std::vector<std::string> snap;
            std::vector<int> mark;
            if (total > 0 && !active_id.empty())
            {
                snap = NodeTextDoc(stack[static_cast<std::size_t>(total - 1 - sel)].json,
                                   active_id);
                mark = DiffMark(snap, cur);
            }

            std::size_t content = 30;
            for (const auto& line : snap) content = std::max(content, line.size());
            for (const auto& st : stack) content = std::max(content, st.preview.size() + 4);
            content = std::min(content, kMaxContent);

            int preview_scroll = 0;
            if (!snap.empty())
            {
                std::size_t first_diff = snap.size();
                for (std::size_t i = 0; i < mark.size() && i < snap.size(); ++i)
                {
                    if (mark[i] != 0)
                    {
                        first_diff = i;
                        break;
                    }
                }
                const int max_scroll = static_cast<int>(snap.size()) - kPreviewRows;
                int target = 0;
                if (first_diff < snap.size())
                {
                    target = static_cast<int>(first_diff) - 2;
                }
                preview_scroll = std::max(0, std::min(std::max(0, max_scroll), target));
            }

            ftxui::Elements preview;
            if (total == 0)
            {
                preview.push_back(ftxui::text(Trunc("  No undo history", content + 2)) | ftxui::dim);
            }
            for (int r = 0; r < kPreviewRows; ++r)
            {
                const int idx = preview_scroll + r;
                std::string line = (idx < static_cast<int>(snap.size()))
                                       ? Trunc(snap[static_cast<std::size_t>(idx)], content)
                                       : "";
                ftxui::Element row = ftxui::text(" " + line + " ");
                if (idx >= 0 && idx < static_cast<int>(mark.size()))
                {
                    if (mark[static_cast<std::size_t>(idx)] == 2)
                    {
                        row |= ftxui::bold | ftxui::color(ftxui::Color::Yellow);
                    }
                    else if (mark[static_cast<std::size_t>(idx)] == 1)
                    {
                        row |= ftxui::color(ftxui::Color::Yellow);
                    }
                }
                preview.push_back(row);
            }

            const int list_top = std::min(sel, std::max(0, total - kVisibleRows));
            ftxui::Elements rows;
            for (int i = 0; i < kVisibleRows; ++i)
            {
                const int pos = list_top + i;
                if (pos >= total) break;
                const std::string label = std::to_string(pos + 1) + ". "
                    + Trunc(stack[static_cast<std::size_t>(total - 1 - pos)].preview,
                            content - 4);
                ftxui::Element row = ftxui::text(" " + label + " ");
                if (pos == sel) row = row | ftxui::inverted;
                rows.push_back(row);
            }
            if (total == 0)
            {
                rows.push_back(ftxui::text(Trunc("  (no undo history)", content + 2)) | ftxui::dim);
            }
            while (static_cast<int>(rows.size()) < kVisibleRows)
            {
                rows.push_back(ftxui::text(Trunc("", content + 2)));
            }

            const std::string footer =
                "  " + std::to_string(total == 0 ? 0 : sel + 1) + "/" + std::to_string(total) +
                "   j/k move  Enter undo  Esc cancel  ";

            const int row_width = static_cast<int>(content) + 2;
            return ftxui::window(ftxui::text(" < Undo "),
                                ftxui::vbox({
                                    ftxui::separator(),
                                    ftxui::vbox(std::move(preview)),
                                    ftxui::separator(),
                                    ftxui::vbox(std::move(rows)),
                                    ftxui::separator(),
                                    ftxui::text(Trunc(footer, static_cast<std::size_t>(row_width))) | ftxui::dim,
                                })) |
                   ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 92) |
                   ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 26);
        }

    private:
        void Close()
        {
            *show_ = false;
            selection_ = 0;
        }

        void Apply()
        {
            const auto& stack = state_->undo_stack;
            if (stack.empty()) return;
            const int sel = std::min(selection_, static_cast<int>(stack.size()) - 1);
            const std::string preview = stack[static_cast<std::size_t>(stack.size() - 1 - sel)].preview;
            Close();
            if (state_->apply_undo)
            {
                state_->apply_undo(static_cast<std::size_t>(sel));
            }
            state_->status = "Undone: " + preview;
        }

        void Move(int dir)
        {
            const int total = static_cast<int>(state_->undo_stack.size());
            if (total == 0) return;
            selection_ = std::max(0, std::min(total - 1, selection_ + dir));
        }

        std::shared_ptr<EditorState> state_;
        bool* show_;
        int selection_ = 0;
        static constexpr int kPreviewRows = 8;
        static constexpr int kVisibleRows = 10;
        static constexpr std::size_t kMaxContent = 88;
    };

    ftxui::Component MakeUndoDialog(std::shared_ptr<EditorState> state, bool* show)
    {
        return ftxui::Make<UndoDialog>(std::move(state), show);
    }
}
