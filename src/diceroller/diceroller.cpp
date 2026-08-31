#include "diceroller.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <list>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <ftxui/dom/elements.hpp>

#include "../editor/Random.hpp"
#include "../treeview/node_type.hpp"

namespace terminadventure::diceroller
{
    namespace
    {
        const int kSafetyLimit = 500;
        const std::size_t kMaxOutputLines = 500;

        std::string Split(std::string s, const std::string& delim, int pos,
                          const std::string& def)
        {
            const std::size_t found = s.find(delim);
            if (found == std::string::npos)
            {
                return def;
            }
            if (pos == 0)
            {
                return s.substr(0, found);
            }
            if (pos == 1)
            {
                return s.substr(found + delim.size());
            }
            return "";
        }

        std::string IntToStr(int n)
        {
            return std::to_string(n);
        }

        std::string RollsHelp()
        {
            std::string h;
            h += "Basic roll -- <dice count>d<face count>\n";
            h += "  2d6   1d100\n\n";
            h += "Multiple rolls -- <roll count>*<basic roll>\n";
            h += "  2*3d6   4*1d8\n\n";
            h += "Best n of n -- (<best>)<basic roll>\n";
            h += "  (2)4d6   2*(3)4d8\n\n";
            h += "Advantage -- <basic roll>+<advantage>\n";
            h += "  3d6+2   2*(2)3d4+1\n\n";
            h += "Penalty -- <basic roll>-<penalty>\n";
            h += "  3d20-2   3*(2)4d6-2";
            return h;
        }

        std::string PresetsHelp()
        {
            std::string h;
            h += "List presets: presets or pls\n";
            h += "Save last roll: save\n";
            h += "Run a preset: preset <n> or p <n>\n";
            h += "Delete a preset: delete <n>";
            return h;
        }

        int RollDie(int sides)
        {
            return Random::get(1, sides);
        }

        std::string DoRolls(int times, int best_of, int dice_num, int dice_sides,
                            int advantage, int penalty)
        {
            std::string out = "--------------------------------------------------------------------------------\n";
            std::string adv = advantage != 0 ? "(+" + IntToStr(advantage) + ") " : "";
            std::string pen = penalty != 0 ? "(-" + IntToStr(penalty) + ") " : "";

            for (int t = 0; t < times; ++t)
            {
                if (!out.empty()) out += "\n\n";
                out += "Roll number " + IntToStr(t + 1) + "\n";

                std::list<int> roll;
                for (int r = 0; r < dice_num; ++r)
                {
                    roll.push_back(RollDie(dice_sides));
                }
                for (int itm : roll)
                {
                    out += "    " + IntToStr(itm) + "\n";
                }

                roll.sort(std::greater<int>());
                int total = 0;

                if (best_of > 0 && best_of < dice_num)
                {
                    out += "Best " + IntToStr(best_of) + ":\n    ";
                    int count = 0;
                    for (int itm : roll)
                    {
                        if (count < best_of)
                        {
                            total += itm;
                            out += IntToStr(itm);
                            if (count < best_of - 1) out += ", ";
                        }
                        ++count;
                    }
                    out += "\n";
                }
                else
                {
                    for (int itm : roll) total += itm;
                }

                total = total + advantage - penalty;
                out += "\nTotal: " + adv + pen + IntToStr(total) + "\n";

                if (times > 1 && t < times - 1) out += "\n----------------\n";
            }

            out += "\n--------------------------------------------------------------------------------\n";
            return out;
        }

        // Parse and roll `rollstring`, returning the human-readable output.
        // Empty string on parse failure (caller shows help).
        std::string ParseRoll(const std::string& in)
        {
            int times = 1;
            int best_of = 0;
            int dice_num = 1;
            int dice_sides = 1;
            int advantage = 0;
            int penalty = 0;
            std::string rollstring = in;

            try
            {
                times = std::stoi(Split(rollstring, "*", 0, "1"));
                if (times > kSafetyLimit) times = kSafetyLimit;
                rollstring = Split(rollstring, "*", 1, rollstring);

                if (Split(rollstring, "(", 0, "NA") != "NA"
                    && Split(rollstring, ")", 0, "NA") != "NA")
                {
                    std::string tmp = Split(rollstring, "(", 1, "");
                    tmp = Split(tmp, ")", 0, "");
                    best_of = std::stoi(tmp);
                    rollstring = Split(rollstring, ")", 1, rollstring);
                }

                if (Split(rollstring, "+", 0, "NA") != "NA")
                {
                    advantage = std::stoi(Split(rollstring, "+", 1, "0"));
                    if (advantage > kSafetyLimit) advantage = kSafetyLimit;
                    rollstring = Split(rollstring, "+", 0, rollstring);
                }

                if (Split(rollstring, "-", 0, "NA") != "NA")
                {
                    penalty = std::stoi(Split(rollstring, "-", 1, "0"));
                    if (penalty > kSafetyLimit) penalty = kSafetyLimit;
                    rollstring = Split(rollstring, "-", 0, rollstring);
                }

                dice_num = std::stoi(Split(rollstring, "d", 0, "0"));
                dice_sides = std::stoi(Split(rollstring, "d", 1, "0"));

                if (dice_num < 1 || dice_sides < 2 || best_of >= dice_num)
                {
                    return "";
                }
            }
            catch (const std::invalid_argument&)
            {
                return "";
            }
            catch (const std::out_of_range&)
            {
                return "";
            }

            return DoRolls(times, best_of, dice_num, dice_sides, advantage, penalty);
        }

        // Render the padded/separated edge line for the roller's visual break.

        class DiceRoller : public ftxui::ComponentBase
        {
        public:
            explicit DiceRoller(std::shared_ptr<EditorState> state)
                : state_(std::move(state))
            {
            }

            bool Focusable() const override { return true; }

            bool OnEvent(ftxui::Event event) override
            {
                if (event == ftxui::Event::Return)
                {
                    Execute();
                    return true;
                }
                if (event == ftxui::Event::Backspace)
                {
                    if (!input_.empty()) input_.pop_back();
                    return true;
                }
                if (event == ftxui::Event::Escape)
                {
                    if (!input_.empty())
                    {
                        input_.clear();
                        return true;
                    }
                    state_->mode = Mode::TREE;
                    if (state_->focus_treeview) state_->focus_treeview();
                    return true;
                }
                if (event.is_character() && !event.character().empty())
                {
                    input_ += event.character();
                    return true;
                }
                return false;
            }

            ftxui::Element Render() override
            {
                ftxui::Elements out;
                if (output_.empty())
                {
                    out.push_back(ftxui::text(
                        "Type a roll, e.g. 2d6 or 2*(3)4d8+2, and press Enter.") | ftxui::dim);
                    out.push_back(ftxui::emptyElement() | ftxui::flex);
                }
                else
                {
                    // Spacer grows to fill the pane, pinning the newest output
                    // (bottom of the list) to the bottom of the editor area.
                    out.push_back(ftxui::emptyElement() | ftxui::flex);
                    for (const std::string& line : output_)
                    {
                        out.push_back(ftxui::text(line));
                    }
                }
                auto output_area = ftxui::vbox(std::move(out)) | ftxui::frame | ftxui::flex;

                auto prompt = ftxui::text("> ");
                auto input_text = ftxui::text(input_ + "\u258f");
                auto input_line = ftxui::hbox({ prompt, input_text })
                              | ftxui::bgcolor(ftxui::Color::GrayDark);

                auto title = ftxui::text("Dice Roller") | ftxui::bold | ftxui::center;
                return ftxui::vbox({
                    title,
                    ftxui::separator(),
                    output_area,
                    ftxui::separator(),
                    input_line,
                });
            }

        private:
            std::shared_ptr<EditorState> state_;
            std::string input_;
            std::deque<std::string> output_;
            std::string last_roll_;

            void AddOutput(const std::string& text)
            {
                for (const std::string& line : SplitLines(text))
                {
                    output_.push_back(line);
                }
                while (output_.size() > kMaxOutputLines)
                {
                    output_.pop_front();
                }
            }

            static std::vector<std::string> SplitLines(const std::string& s)
            {
                std::vector<std::string> lines;
                std::string cur;
                for (char c : s)
                {
                    if (c == '\n')
                    {
                        lines.push_back(cur);
                        cur.clear();
                    }
                    else
                    {
                        cur += c;
                    }
                }
                if (!cur.empty()) lines.push_back(cur);
                return lines;
            }

            void Execute()
            {
                const std::string cmd = input_;
                input_.clear();
                output_.clear();

                const std::string trimmed = Trim(cmd);
                if (trimmed.empty() || trimmed == "?")
                {
                    AddOutput("Roll help:");
                    AddOutput("  " + RollsHelp());
                    AddOutput("");
                    AddOutput("Presets help:");
                    AddOutput("  " + PresetsHelp());
                    return;
                }
                if (trimmed == "? rolls")
                {
                    AddOutput(RollsHelp());
                    return;
                }
                if (trimmed == "? presets")
                {
                    AddOutput(PresetsHelp());
                    return;
                }
                if (trimmed == "presets" || trimmed == "pls")
                {
                    ListPresets();
                    return;
                }
                if (trimmed == "save")
                {
                    SavePreset();
                    return;
                }
                if (trimmed.rfind("preset ", 0) == 0 || trimmed.rfind("p ", 0) == 0
                    || trimmed == "p")
                {
                    RunPreset(trimmed);
                    return;
                }
                if (trimmed.rfind("delete ", 0) == 0)
                {
                    DeletePreset(trimmed);
                    return;
                }

                const std::string result = ParseRoll(trimmed);
                if (result.empty())
                {
                    AddOutput("Invalid roll: \"" + trimmed + "\"");
                    AddOutput(RollsHelp());
                    return;
                }
                last_roll_ = trimmed;
                AddOutput(result);
            }

            static std::string Trim(const std::string& s)
            {
                std::size_t b = 0, e = s.size();
                while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
                while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
                return s.substr(b, e - b);
            }

            bool ToIndex(const std::string& str, int& out)
            {
                try
                {
                    out = std::stoi(str);
                    return out >= 1;
                }
                catch (const std::exception&)
                {
                    return false;
                }
            }

            void ListPresets()
            {
                if (state_->presets.empty())
                {
                    AddOutput("There are no presets saved.");
                    return;
                }
                AddOutput("Presets:");
                for (std::size_t i = 0; i < state_->presets.size(); ++i)
                {
                    AddOutput("  " + IntToStr(static_cast<int>(i + 1)) + ": " + state_->presets[i]);
                }
            }

            void SavePreset()
            {
                if (last_roll_.empty())
                {
                    AddOutput("No previous roll to save.");
                    return;
                }
                for (const std::string& p : state_->presets)
                {
                    if (p == last_roll_)
                    {
                        AddOutput("The preset already exists.");
                        return;
                    }
                }
                state_->presets.push_back(last_roll_);
                state_->changed = true;
                AddOutput("Preset saved: " + last_roll_);
            }

            void RunPreset(const std::string& cmd)
            {
                const std::size_t sp = cmd.find(' ');
                if (sp == std::string::npos)
                {
                    AddOutput("To run a preset use: preset <number> or p <number>");
                    return;
                }
                int idx = 0;
                if (!ToIndex(Trim(cmd.substr(sp + 1)), idx)) return;
                if (idx < 1 || static_cast<std::size_t>(idx) > state_->presets.size())
                {
                    AddOutput("No preset " + IntToStr(idx) + ".");
                    return;
                }
                const std::string roll = state_->presets[static_cast<std::size_t>(idx - 1)];
                AddOutput("Running preset: " + roll);
                const std::string result = ParseRoll(roll);
                if (result.empty())
                {
                    AddOutput("Invalid preset: \"" + roll + "\"");
                    return;
                }
                last_roll_ = roll;
                AddOutput(result);
            }

            void DeletePreset(const std::string& cmd)
            {
                const std::size_t sp = cmd.find(' ');
                if (sp == std::string::npos)
                {
                    AddOutput("To delete a preset use: delete <number>");
                    return;
                }
                int idx = 0;
                if (!ToIndex(Trim(cmd.substr(sp + 1)), idx)) return;
                if (idx < 1 || static_cast<std::size_t>(idx) > state_->presets.size())
                {
                    AddOutput("No preset " + IntToStr(idx) + ".");
                    return;
                }
                state_->presets.erase(state_->presets.begin() + static_cast<std::ptrdiff_t>(idx - 1));
                state_->changed = true;
                AddOutput("Preset deleted.");
            }
        };
    }

    ftxui::Component MakeDiceRoller(std::shared_ptr<EditorState> state)
    {
        return ftxui::Make<DiceRoller>(std::move(state));
    }
}
