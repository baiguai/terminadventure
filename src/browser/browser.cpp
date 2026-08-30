#include "browser.hpp"

#include <algorithm>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>

#include "../editor/editor_state.hpp"
#include "../op/op.hpp"

namespace fs = std::filesystem;

namespace terminadventure::browser
{
    namespace
    {
        std::string PadRight(const std::string& s, std::size_t width)
        {
            if (s.size() >= width) return s;
            return s + std::string(width - s.size(), ' ');
        }

        std::string Tail(const std::string& s, std::size_t width)
        {
            if (s.size() <= width) return s;
            return s.substr(s.size() - width);
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

        bool NoCaseLess(const std::string& a, const std::string& b)
        {
            return Lower(a) < Lower(b);
        }

        std::string Normalize(const std::string& path)
        {
            std::error_code ec;
            fs::path abs = fs::absolute(path, ec);
            if (ec) return path;
            return abs.string();
        }
    }

    class FileBrowserDialog : public ftxui::ComponentBase
    {
    public:
        FileBrowserDialog(std::shared_ptr<EditorState> state, bool* show)
            : state_(std::move(state)), show_(show) {}

        bool Focusable() const override { return true; }

        bool OnEvent(ftxui::Event event) override
        {
            if (filter_active_) return OnFilterEvent(event);

            if (event == ftxui::Event::Escape)
            {
                if (!filter_.empty())
                {
                    ClearFilter();
                    return true;
                }
                Close();
                return true;
            }
            if (event.is_character() && event.character() == "/")
            {
                filter_active_ = true;
                return true;
            }
            if (event.is_character() && event.character() == ":"
                && !state_->browser_command.empty())
            {
                // The dialog just picks a directory; `:` hands the currently
                // shown directory back to the save/export command line so the
                // user can type a filename.
                const std::string command = std::move(state_->browser_command);
                state_->browser_command.clear();
                const std::string target = (fs::path(dir_) / "").string();
                Close();
                terminadventure::op::OpenCommandLineWithArgs(state_, command, target);
                return true;
            }
            if (event == ftxui::Event::Return)
            {
                Activate();
                return true;
            }
            if (event.is_character() && event.character() == "l")
            {
                EnterAt(/*pick_file=*/false);
                return true;
            }
            if (event == ftxui::Event::ArrowDown
                || (event.is_character() && event.character() == "j"))
            {
                MoveSelection(+1);
                pending_g_ = false;
                return true;
            }
            if (event == ftxui::Event::ArrowUp
                || (event.is_character() && event.character() == "k"))
            {
                MoveSelection(-1);
                pending_g_ = false;
                return true;
            }
            if (event == ftxui::Event::Backspace
                || (event.is_character() && event.character() == "h"))
            {
                GoUp();
                pending_g_ = false;
                return true;
            }
            if (event.is_character() && event.character() == "g")
            {
                if (pending_g_) JumpToTop();
                else pending_g_ = true;
                return true;
            }
            if (event.is_character() && event.character() == "G")
            {
                JumpToBottom();
                return true;
            }
            pending_g_ = false;
            return true;
        }

        ftxui::Element Render() override
        {
            Recompute();

            const int total = static_cast<int>(visible_.size());
            const int sel = std::min(selection_, std::max(0, total - 1));
            const int top = ComputeTop(sel, total);
            const int count = std::min(kVisibleRows, std::max(0, total - top));
            const int dlg_width { 90 };

            const std::size_t row_width = static_cast<std::size_t>(content_width_ + 2);

            ftxui::Elements rows;
            if (filter_active_)
            {
                rows.push_back(ftxui::text(
                    PadRight(Tail("  / " + filter_ + "_  ", content_width_ + 2), content_width_ + 2))
                               | ftxui::inverted);
            }
            rows.push_back(ftxui::text(PadRight("  " + dir_, content_width_ + 2)) | ftxui::dim);
            rows.push_back(ftxui::separator());

            if (total == 0)
            {
                rows.push_back(ftxui::text(PadRight(
                    filter_.empty() ? "  (empty directory)" : "  (no matches)", row_width))
                               | ftxui::dim);
            }
            else
            {
                for (int i = 0; i < count; ++i)
                {
                    const int index = top + i;
                    ftxui::Element row = ftxui::text(
                        " " + PadRight(entries_[visible_[static_cast<std::size_t>(index)]].display,
                                       static_cast<std::size_t>(content_width_))
                        + " ");
                    if (index == sel) row = row | ftxui::inverted;
                    rows.push_back(row);
                }
            }
            const int pad_to = kVisibleRows + 3;
            while (static_cast<int>(rows.size()) < pad_to)
            {
                rows.push_back(ftxui::text(PadRight("", row_width)));
            }

            std::string help_move =
                "    j/k move  h up  l enter  gg/G  Enter pick  / filter  Esc cancel  ";
            if (!state_->browser_command.empty())
            {
                help_move =
                    "    j/k move  h up  l enter  gg/G  Enter pick  : name  / filter  Esc cancel  ";
            }
            const std::string help_clear =
                "    Esc clear  j/k move  h up  l enter  Enter pick  / filter  ";
            const std::string help_type = "    type to filter  Enter keep  Esc clear  ";

            std::string footer = "  " + std::to_string(total == 0 ? 0 : sel + 1) + "/"
                                 + std::to_string(total);
            if (!filter_.empty())
            {
                footer += " (of " + std::to_string(entries_.size()) + ")";
            }
            if (filter_active_)
            {
                footer += help_type;
            }
            else if (filter_.empty())
            {
                footer += help_move;
            }
            else
            {
                footer += help_clear;
            }

            return ftxui::window(ftxui::text(" File Browser "),
                                 ftxui::vbox({
                                     ftxui::vbox(std::move(rows)),
                                     ftxui::separator(),
                                     ftxui::text(PadRight(footer, dlg_width)) | ftxui::dim,
                                 })) |
                   ftxui::reflect(box_) |
                   ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 92) |
                   ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 24);
        }

    private:
        static constexpr int kVisibleRows = 16;

        struct Entry
        {
            bool is_dir = false;
            std::string display;
            std::string path;
        };

        void Close()
        {
            *show_ = false;
            selection_ = 0;
            scroll_ = 0;
            pending_g_ = false;
            filter_.clear();
            filter_active_ = false;
            state_->browser_command.clear();
            start_dir_.clear();
            needs_refresh_ = true;
        }

        void SetDir(const std::string& path)
        {
            dir_ = Normalize(path);
            needs_refresh_ = true;
            pending_g_ = false;
        }

        void MoveSelection(int dir)
        {
            if (visible_.empty()) return;
            const int total = static_cast<int>(visible_.size());
            selection_ = std::max(0, std::min(total - 1, selection_ + dir));
        }

        void JumpToTop()
        {
            if (visible_.empty()) return;
            selection_ = 0;
            pending_g_ = false;
        }

        void JumpToBottom()
        {
            if (visible_.empty()) return;
            selection_ = static_cast<int>(visible_.size()) - 1;
            pending_g_ = false;
        }

        void GoUp()
        {
            const fs::path current(dir_);
            const fs::path parent = current.parent_path();
            if (parent == current) return;
            SetDir(parent.string());
            ClearFilter();
        }

        void Activate()
        {
            EnterAt(/*pick_file=*/true);
        }

        void EnterAt(bool pick_file)
        {
            if (visible_.empty()) return;
            const int sel = std::min(selection_, static_cast<int>(visible_.size()) - 1);
            const Entry& entry = entries_[visible_[static_cast<std::size_t>(sel)]];
            if (entry.is_dir)
            {
                // `l` always navigates into the folder. For save/export-style
                // ops (`browser_command` set) the dialog is a folder picker:
                // Enter hands the chosen folder back to the command line so a
                // filename can be typed. `:` picks the currently shown folder
                // the same way.
                if (!pick_file || state_->browser_command.empty())
                {
                    SetDir(entry.path);
                    ClearFilter();
                    return;
                }
                const std::string command = std::move(state_->browser_command);
                state_->browser_command.clear();
                const std::string target = (fs::path(entry.path) / "").string();
                Close();
                terminadventure::op::OpenCommandLineWithArgs(state_, command, target);
                return;
            }
            if (!pick_file) return;
            if (!state_->browser_pick) return;
            const std::string picked = entry.path;
            const auto callback = std::move(state_->browser_pick);
            state_->browser_pick = {};
            Close();
            callback(picked);
        }

        void ClearFilter()
        {
            filter_.clear();
            filter_active_ = false;
            selection_ = 0;
        }

        int ComputeTop(int sel, int total) const
        {
            const int max_top = std::max(0, total - kVisibleRows);
            int top = std::min(scroll_, max_top);
            if (sel < top) top = sel;
            if (sel >= top + kVisibleRows) top = sel - kVisibleRows + 1;
            return std::max(0, std::min(top, max_top));
        }

        bool OnFilterEvent(ftxui::Event event)
        {
            if (event == ftxui::Event::Escape)
            {
                ClearFilter();
                return true;
            }
            if (event == ftxui::Event::Return)
            {
                filter_active_ = false;
                selection_ = 0;
                return true;
            }
            if (event == ftxui::Event::Backspace)
            {
                if (!filter_.empty()) filter_.pop_back();
                selection_ = 0;
                return true;
            }
            if (event.is_character())
            {
                filter_ += event.character();
                selection_ = 0;
                return true;
            }
            return true;
        }

        void Recompute()
        {
            if (state_->browser_start_dir != start_dir_)
            {
                start_dir_ = state_->browser_start_dir;
                filter_.clear();
                filter_active_ = false;
                SetDir(start_dir_);
                selection_ = 0;
            }
            if (needs_refresh_)
            {
                needs_refresh_ = false;
                Relist();
            }
            if (last_filter_ != filter_) ApplyFilter();
        }

        void Relist()
        {
            entries_.clear();
            content_width_ = 24;

            std::error_code ec;
            const fs::path dir(dir_);

            const fs::path parent = dir.parent_path();
            if (parent != dir)
            {
                entries_.push_back(Entry{true, "../", parent.string()});
            }

            std::vector<std::string> subdirs;
            std::vector<std::string> files;
            for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec),
                 end;
                 it != end; it.increment(ec))
            {
                if (ec) break;
                std::error_code sec;
                const fs::path p = it->path();
                const std::string name = p.filename().string();
                if (name.empty() || name == ".") continue;
                if (fs::is_directory(p, sec)) subdirs.push_back(name);
                else files.push_back(name);
            }
            (void)ec;

            std::sort(subdirs.begin(), subdirs.end(), NoCaseLess);
            std::sort(files.begin(), files.end(), NoCaseLess);

            for (const std::string& name : subdirs)
            {
                entries_.push_back(Entry{true, name + "/", (dir / name).string()});
            }
            for (const std::string& name : files)
            {
                entries_.push_back(Entry{false, name, (dir / name).string()});
            }

            for (const Entry& e : entries_)
            {
                content_width_ = std::max(content_width_, static_cast<int>(e.display.size()));
            }

            ApplyFilter();
        }

        void ApplyFilter()
        {
            last_filter_ = filter_;
            visible_.clear();
            const std::string needle = Lower(filter_);
            for (std::size_t i = 0; i < entries_.size(); ++i)
            {
                if (needle.empty()
                    || Lower(entries_[i].display).find(needle) != std::string::npos)
                {
                    visible_.push_back(i);
                }
            }
            const int total = static_cast<int>(visible_.size());
            selection_ = std::max(0, std::min(selection_, total - 1));
        }

        std::shared_ptr<EditorState> state_;
        bool* show_;
        ftxui::Box box_;
        std::string dir_;
        std::string start_dir_;
        bool needs_refresh_ = true;
        std::vector<Entry> entries_;
        std::vector<std::size_t> visible_;
        std::string filter_;
        std::string last_filter_;
        bool filter_active_ = false;
        int selection_ = 0;
        int scroll_ = 0;
        int content_width_ = 24;
        bool pending_g_ = false;
    };

    ftxui::Component MakeFileBrowserDialog(std::shared_ptr<EditorState> state, bool* show)
    {
        return ftxui::Make<FileBrowserDialog>(std::move(state), show);
    }
}
