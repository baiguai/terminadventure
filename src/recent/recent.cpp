#include "recent.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "../config/config.hpp"
#include "../editor/editor_state.hpp"

namespace terminadventure::recent
{
    namespace
    {
        std::string PadRight(const std::string& s, std::size_t width)
        {
            if (s.size() >= width) return s;
            return s + std::string(width - s.size(), ' ');
        }
    }

    class RecentDialog : public ftxui::ComponentBase
    {
    public:
        RecentDialog(std::shared_ptr<EditorState> state, bool* show,
                     std::shared_ptr<bool> force)
            : state_(std::move(state)), show_(show), force_flag_(std::move(force)) {}

        bool Focusable() const override { return true; }

        bool OnEvent(ftxui::Event event) override
        {
            PickupForce();
            if (event == ftxui::Event::Escape)
            {
                Close();
                return true;
            }
            if (event == ftxui::Event::Return)
            {
                Open();
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
            if (event.is_character() && event.character() == "D")
            {
                RemoveSelected();
                return true;
            }
            if (event.is_character() && event.character() == "!")
            {
                force_ = !force_;
                state_->status = force_ ? "Force open - unsaved changes will be discarded"
                                        : "";
                return true;
            }
            if (event.is_character() && event.character() == "J")
            {
                MoveEntry(+1);
                return true;
            }
            if (event.is_character() && event.character() == "K")
            {
                MoveEntry(-1);
                return true;
            }
            return true; // consume everything else
        }

        ftxui::Element Render() override
        {
            PickupForce();
            const auto& recent = state_->recent_files;
            const int total = static_cast<int>(recent.size());
            const int sel = std::min(selection_, std::max(0, total - 1));

            const int max_top = std::max(0, total - kVisibleRows);
            int top = std::min(scroll_, max_top);
            if (sel < top) top = sel;
            if (sel >= top + kVisibleRows) top = sel - kVisibleRows + 1;
            top = std::max(0, std::min(top, max_top));
            const int count = std::min(kVisibleRows, std::max(0, total - top));

            std::size_t width = 20;
            for (const auto& p : recent) width = std::max(width, p.size());
            const int content_width = static_cast<int>(std::min(width, kMaxContent));

            ftxui::Elements rows;
            const int row_width = content_width + 2;
            if (total == 0)
            {
                rows.push_back(ftxui::text(PadRight("  No recent files", row_width)) | ftxui::dim);
            }
            else
            {
                for (int i = 0; i < count; ++i)
                {
                    ftxui::Element row = ftxui::text(" " + PadRight(recent[static_cast<std::size_t>(top + i)], content_width) + " ");
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
                "   j/k move  J/K reorder  D remove  " +
                std::string(force_ ? "!force-on " : "! force ") +
                "Enter open  Esc cancel  ";

            return ftxui::window(ftxui::text(" < Recent Files "),
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
        void PickupForce()
        {
            if (force_flag_ && *force_flag_)
            {
                force_ = true;
                *force_flag_ = false;
            }
        }

        void Close()
        {
            *show_ = false;
            selection_ = 0;
            scroll_ = 0;
            force_ = false;
        }

        void Open()
        {
            const auto& recent = state_->recent_files;
            if (recent.empty()) return;
            const int sel = std::min(selection_, static_cast<int>(recent.size()) - 1);
            const std::string chosen = recent[static_cast<std::size_t>(sel)];
            const bool force = force_;
            state_->status = "";
            Close();
            auto it = state_->operations.find(force ? "open_force" : "open");
            if (it != state_->operations.end())
            {
                it->second(chosen, 1);
            }
        }

        void MoveSelection(int dir)
        {
            const auto& recent = state_->recent_files;
            if (recent.empty()) return;
            const int total = static_cast<int>(recent.size());
            selection_ = std::max(0, std::min(total - 1, selection_ + dir));
        }

        void RemoveSelected()
        {
            auto& recent = state_->recent_files;
            if (recent.empty()) return;
            const int sel = std::min(selection_, static_cast<int>(recent.size()) - 1);
            recent.erase(recent.begin() + sel);
            selection_ = std::max(0, std::min(selection_, static_cast<int>(recent.size()) - 1));
            scroll_ = 0;
            if (!state_->init_path.empty())
            {
                terminadventure::config::WriteRecentFiles(state_->init_path, recent);
            }
            state_->status = "Recent entry removed";
        }

        void MoveEntry(int dir)
        {
            auto& recent = state_->recent_files;
            if (recent.empty()) return;
            const int sel = std::min(selection_, static_cast<int>(recent.size()) - 1);
            const int other = sel + dir;
            if (other < 0 || other >= static_cast<int>(recent.size())) return;
            std::swap(recent[static_cast<std::size_t>(sel)],
                    recent[static_cast<std::size_t>(other)]);
            selection_ = other;
            if (!state_->init_path.empty())
            {
                terminadventure::config::WriteRecentFiles(state_->init_path, recent);
            }
        }

        std::shared_ptr<EditorState> state_;
        bool* show_;
        std::shared_ptr<bool> force_flag_;
        int selection_ = 0;
        int scroll_ = 0;
        bool force_ = false;
        static constexpr int kVisibleRows = 18;
        static constexpr std::size_t kMaxContent = 88;
    };

    ftxui::Component MakeRecentDialog(std::shared_ptr<EditorState> state, bool* show,
                                      std::shared_ptr<bool> force)
    {
        return ftxui::Make<RecentDialog>(std::move(state), show, std::move(force));
    }

    std::size_t PruneRecentFiles(std::vector<std::string>& recent_files)
    {
        const std::size_t before = recent_files.size();
        std::error_code ec;
        recent_files.erase(
                std::remove_if(recent_files.begin(), recent_files.end(),
                    [&ec](const std::string& path)
                    {
                        return !std::filesystem::exists(path, ec);
                    }),
                recent_files.end());
        return before - recent_files.size();
    }
}
