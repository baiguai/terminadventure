#include "search.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <regex>
#include <utility>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "../editor/editor_state.hpp"

namespace terminadventure::search
{
    namespace
    {
        std::string Lower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return s;
        }

        std::string PadRight(const std::string& s, std::size_t width)
        {
            if (s.size() >= width) return s;
            return s + std::string(width - s.size(), ' ');
        }

        std::string IndentName(int depth, const std::string& name)
        {
            return std::string(static_cast<std::size_t>(depth) * 2, ' ') + name;
        }

        struct Result
        {
            treeview::TreeNode* node;
            std::string line;
        };

        // Parsed search query: a leading "r:" selects case-insensitive regex
        // matching and a leading ":" restricts the match to node titles.
        struct Filter
        {
            std::string query;
            bool is_regex = false;
            bool title_only = false;
        };

        Filter ParseFilter(const std::string& raw)
        {
            Filter f;
            f.query = raw;
            if (f.query.size() >= 2 && f.query[0] == 'r' && f.query[1] == ':')
            {
                f.is_regex = true;
                f.query = f.query.substr(2);
            }
            if (!f.query.empty() && f.query[0] == ':')
            {
                f.title_only = true;
                f.query = f.query.substr(1);
            }
            return f;
        }

        // Case-insensitive match of a node against a parsed filter. An empty
        // query matches everything. When `query` is a regex that fails to
        // compile, returns false and sets `*regex_error`.
        bool NodeMatches(const treeview::TreeNode& node, const Filter& f,
                         bool* regex_error)
        {
            if (f.query.empty()) return true;
            if (f.is_regex)
            {
                try
                {
                    const std::regex re(f.query, std::regex::icase | std::regex_constants::multiline);
                    return std::regex_search(node.name, re)
                        || (!f.title_only && std::regex_search(node.text, re));
                }
                catch (const std::regex_error&)
                {
                    if (regex_error) *regex_error = true;
                    return false;
                }
            }
            const std::string needle = Lower(f.query);
            return Lower(node.name).find(needle) != std::string::npos
                || (!f.title_only && Lower(node.text).find(needle) != std::string::npos);
        }

        // A tag that is purely hex digits of length 3, 4, 6 or 8 (e.g. "#fff"
        // or "#ff8800") is an HTML color, not a tag.
        bool IsHexColor(const std::string& tag)
        {
            const std::size_t n = tag.size();
            if (n != 3 && n != 4 && n != 6 && n != 8) return false;
            return std::all_of(tag.begin(), tag.end(), [](unsigned char c) {
                return std::isxdigit(c) != 0;
            });
        }

        // Collect `#tag` tokens from `text` into `counts` (keyed by lowercase
        // tag, so the map iteration is sorted and de-duplicated). HTML color
        // codes such as "#fff" or "#ff8800" are ignored.
        void CollectTags(const std::string& text, std::map<std::string, int>& counts)
        {
            static const std::regex kTagRegex(R"(#[\w-]+)");
            for (std::sregex_iterator it(text.begin(), text.end(), kTagRegex), end;
                 it != end; ++it)
            {
                const std::string tag = Lower(it->str().substr(1));
                if (IsHexColor(tag)) continue;
                ++counts[tag];
            }
        }

        bool ContainsTag(const std::string& text, const std::string& tag)
        {
            static const std::regex kTagRegex(R"(#[\w-]+)");
            for (std::sregex_iterator it(text.begin(), text.end(), kTagRegex), end;
                 it != end; ++it)
            {
                if (Lower(it->str().substr(1)) == tag) return true;
            }
            return false;
        }
    }

    std::vector<treeview::TreeNode*> FindMatches(std::shared_ptr<EditorState> state,
                                                 const std::string& raw_query)
    {
        std::vector<treeview::TreeNode*> out;
        if (state->active_node == nullptr) return out;
        const Filter f = ParseFilter(raw_query);
        bool regex_error = false;
        if (NodeMatches(*state->active_node, f, &regex_error))
        {
            out.push_back(state->active_node);
        }
        return out;
    }

    std::vector<std::pair<int, int>> FindLineMatches(const std::string& line,
                                                     const std::string& raw_query)
    {
        const Filter f = ParseFilter(raw_query);
        if (f.title_only || f.query.empty()) return {};

        std::vector<std::pair<int, int>> out;
        if (f.is_regex)
        {
            try
            {
                const std::regex re(f.query, std::regex::icase);
                for (std::sregex_iterator it(line.begin(), line.end(), re), end;
                     it != end; ++it)
                {
                    out.emplace_back(static_cast<int>(it->position()),
                                     static_cast<int>(it->position() + it->length()));
                }
            }
            catch (const std::regex_error&)
            {
                return {};
            }
            return out;
        }

        const std::string needle = Lower(f.query);
        const std::string hay = Lower(line);
        std::size_t pos = 0;
        while ((pos = hay.find(needle, pos)) != std::string::npos)
        {
            out.emplace_back(static_cast<int>(pos),
                             static_cast<int>(pos + needle.size()));
            pos += needle.size();
        }
        return out;
    }

    class SearchDialog : public ftxui::ComponentBase
    {
    public:
        SearchDialog(std::shared_ptr<EditorState> state, bool* show,
                     bool insert_mode)
            : state_(std::move(state)),
              show_(show),
              insert_mode_(insert_mode) {}

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
                if (tag_phase_ && !results_.empty())
                {
                    const int sel = std::min(selection_, static_cast<int>(results_.size()) - 1);
                    filter_ = results_[static_cast<std::size_t>(sel)].line;
                    tag_phase_ = false;
                    Invalidate();
                    return true;
                }
                if (!results_.empty())
                {
                    const int sel = std::min(selection_, static_cast<int>(results_.size()) - 1);
                    treeview::TreeNode* node = results_[static_cast<std::size_t>(sel)].node;
                    if (insert_mode_)
                    {
                        if (state_->insert_text_at_cursor && node != nullptr)
                        {
                            state_->insert_text_at_cursor("_" + node->name + "_");
                            state_->status = "Inserted " + node->name;
                        }
                    }
                    else if (state_->reveal_node)
                    {
                        state_->reveal_node(node);
                        state_->status = "";
                    }
                    Close();
                }
                return true;
            }
            if (event == ftxui::Event::ArrowDown)
            {
                MoveSelection(+1);
                return true;
            }
            if (event == ftxui::Event::ArrowUp)
            {
                MoveSelection(-1);
                return true;
            }
            if (event == ftxui::Event::Backspace)
            {
                if (!filter_.empty())
                {
                    filter_.pop_back();
                    Invalidate();
                }
                return true;
            }
            if (event.is_character())
            {
                filter_ += event.character();
                Invalidate();
                return true;
            }
            return true;
        }

        ftxui::Element Render() override
        {
            if (!results_valid_) Recompute();

            const int total = static_cast<int>(results_.size());
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
                std::string msg;
                if (regex_error_) msg = "  Invalid regex";
                else if (filter_.empty()) msg = "  No nodes in the document";
                else msg = "  No matches";
                rows.push_back(ftxui::text(PadRight(msg, row_width)) | ftxui::dim);
            }
            else
            {
                for (int i = 0; i < count; ++i)
                {
                    ftxui::Element row = ftxui::text(" " + PadRight(results_[static_cast<std::size_t>(top + i)].line, content_width_) + " ");
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
                (insert_mode_
                     ? "    Up/Down move  Enter insert _Title_  Esc cancel  ':x' = titles only  'r:' = regex  '#' = tags  "
                     : "    Up/Down move  Enter jump  Esc cancel  ':x' = titles only  'r:' = regex  '#' = tags  ");

            return ftxui::window(ftxui::text(insert_mode_ ? " / Insert Link " : " / Search "),
                                ftxui::vbox({
                                    ftxui::hbox({
                                        ftxui::text(" Search: " + filter_ + "_"),
                                        ftxui::filler(),
                                    }),
                                    ftxui::separator(),
                                    ftxui::vbox(std::move(rows)),
                                    ftxui::separator(),
                                    ftxui::text(PadRight(footer, row_width)) | ftxui::dim,
                                })) |
                   ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 190) |
                   ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 24);
        }

    private:
        void Invalidate()
        {
            results_valid_ = false;
            selection_ = 0;
            scroll_ = 0;
        }

        void Close()
        {
            *show_ = false;
            filter_.clear();
            selection_ = 0;
            scroll_ = 0;
            regex_error_ = false;
            tag_phase_ = false;
            results_valid_ = false;
        }

        void MoveSelection(int dir)
        {
            if (results_.empty()) return;
            const int total = static_cast<int>(results_.size());
            selection_ = std::max(0, std::min(total - 1, selection_ + dir));
        }

        void Recompute()
        {
            results_.clear();
            regex_error_ = false;
            tag_phase_ = false;

            std::vector<std::pair<treeview::TreeNode*, int>> all;
            if (state_->collect_all_nodes)
            {
                all = state_->collect_all_nodes();
            }

            content_width_ = 72;
            for (const auto& item : all)
            {
                content_width_ = std::max(content_width_,
                                          static_cast<int>(IndentName(item.second, item.first->name).size()));
            }

            if (!filter_.empty() && filter_[0] == '#')
            {
                RecomputeTags(all);
            }
            else
            {
                const Filter f = ParseFilter(filter_);
                bool regex_error = false;
                for (const auto& item : all)
                {
                    if (NodeMatches(*item.first, f, &regex_error))
                    {
                        results_.push_back(Result{item.first, IndentName(item.second, item.first->name)});
                    }
                }
                if (regex_error) regex_error_ = true;
            }

            selection_ = 0;
            scroll_ = 0;
            results_valid_ = true;
        }

        // Tag search: a leading '#' lists matching tags; picking one (or an
        // exact match) lists the nodes whose content carries that tag.
        void RecomputeTags(const std::vector<std::pair<treeview::TreeNode*, int>>& all)
        {
            const std::string typed = Lower(filter_.substr(1));

            std::map<std::string, int> counts;
            for (const auto& item : all)
            {
                CollectTags(item.first->text, counts);
            }

            if (!typed.empty() && counts.find(typed) != counts.end())
            {
                for (const auto& item : all)
                {
                    if (ContainsTag(item.first->text, typed))
                    {
                        results_.push_back(Result{item.first, IndentName(item.second, item.first->name)});
                    }
                }
                return;
            }

            tag_phase_ = true;
            for (const auto& entry : counts)
            {
                if (entry.first.find(typed) != std::string::npos)
                {
                    const std::string line = "#" + entry.first;
                    results_.push_back(Result{nullptr, line});
                    content_width_ = std::max(content_width_, static_cast<int>(line.size()));
                }
            }
        }

        std::shared_ptr<EditorState> state_;
        bool* show_;
        bool insert_mode_;
        std::string filter_;
        int selection_ = 0;
        int scroll_ = 0;
        int content_width_ = 72;
        bool results_valid_ = false;
        bool regex_error_ = false;
        bool tag_phase_ = false;
        std::vector<Result> results_;
        static constexpr int kVisibleRows = 18;
    };

    ftxui::Component MakeSearchDialog(std::shared_ptr<EditorState> state, bool* show,
                                      bool insert_mode)
    {
        return ftxui::Make<SearchDialog>(std::move(state), show, insert_mode);
    }
}
