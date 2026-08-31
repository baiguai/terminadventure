#include "players.hpp"

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <ftxui/dom/elements.hpp>

#include "../editor/Random.hpp"
#include "../editor/editor_state.hpp"

namespace terminadventure::players
{
    namespace
    {
        using ftxui::Elements;
        using ftxui::Element;
        using ftxui::text;

        std::string Trim(const std::string& s)
        {
            std::size_t b = 0, e = s.size();
            while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
            while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
            return s.substr(b, e - b);
        }

        std::string PadTo(const std::string& s, std::size_t n)
        {
            if (s.size() >= n) return s;
            return s + std::string(n - s.size(), ' ');
        }

        std::string Lower(std::string s)
        {
            for (char& c : s)
            {
                if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
            }
            return s;
        }

        std::vector<std::string> Split(const std::string& s, char sep)
        {
            std::vector<std::string> out;
            std::string cur;
            for (char c : s)
            {
                if (c == sep) { out.push_back(cur); cur.clear(); }
                else cur += c;
            }
            out.push_back(cur);
            out.erase(std::remove(out.begin(), out.end(), ""), out.end());
            return out;
        }

        static std::string Join(const std::vector<std::string>& v, const std::string& sep)
        {
            std::string out;
            for (size_t i = 0; i < v.size(); ++i)
            {
                if (i) out += sep;
                out += v[i];
            }
            return out;
        }

        std::string FirstNames[20] = {
            "Aldric","Brenna","Cedric","Dara","Eldon","Fiona","Gideon","Hilda","Idris","Jessa",
            "Kael","Lyra","Magnus","Nora","Osric","Petra","Quinn","Rowan","Seren","Torvin"
        };
        std::string LastNames[20] = {
            "Blackwood","Brightmoon","Coldwater","Darkhollow","Elderwood","Fireforge","Goldleaf",
            "Hillcrest","Ironforge","Jadewind","Keenblade","Lightfoot","Moonwhisper","Nightsky",
            "Oakenshield","Proudfoot","Quicksilver","Ravencrest","Silverstream","Thornwood"
        };

        std::string RandomName()
        {
            return FirstNames[Random::get(0, 19)] + " " + LastNames[Random::get(0, 19)];
        }

        int Roll4d6DropLowest()
        {
            int a = Random::get(1, 6), b = Random::get(1, 6);
            int c = Random::get(1, 6), d = Random::get(1, 6);
            return a + b + c + d - std::min({a, b, c, d});
        }

        int RollHitPoints(int hit_die, int level, int con_mod)
        {
            int total = 0;
            for (int i = 0; i < level; ++i) total += Random::get(1, hit_die) + con_mod;
            return std::max(total, level * (1 + con_mod));
        }

        int StatValue(const Player& p, const std::string& stat)
        {
            if (stat == "STR") return p.str;
            if (stat == "DEX") return p.dex;
            if (stat == "CON") return p.con;
            if (stat == "INT") return p.intel;
            if (stat == "WIS") return p.wis;
            return p.cha;
        }

        // Focus zones.
        enum Zone { ZoneList = 0, ZoneActions = 1, ZoneForm = 2, ZoneCount = 3 };

        enum class Kind { Input, Number, Dropdown, Checkbox, Button };

        struct Widget
        {
            Kind kind = Kind::Input;
            std::string label;
            std::string* text = nullptr;      // Input text / dropdown current text
            int* number = nullptr;            // Number input
            std::vector<std::string> options; // Dropdown options
            std::function<bool()> checked;    // Checkbox state
            std::function<void()> toggle;     // Checkbox toggle
            std::function<void()> on_change;  // Dropdown/Number change callback
            std::function<void()> on;         // Button
            std::function<std::string()> suffix; // Number: text shown after the value
            std::string hint;
        };

        class PlayersPane : public ftxui::ComponentBase
        {
        public:
            explicit PlayersPane(std::shared_ptr<EditorState> state)
                : state_(std::move(state))
            {
            }

            bool Focusable() const override { return true; }

            bool OnEvent(ftxui::Event event) override
            {
                if (confirming_delete_) return OnConfirmEvent(event);
                if (dropdown_open_) return OnDropdownEvent(event);
                if (which_ == ZoneActions) return OnActionZoneEvent(event);

                if (event == ftxui::Event::Escape)
                {
                    if (which_ == ZoneList && !search_.empty()) { search_.clear(); return true; }
                    if (which_ != ZoneList) { which_ = ZoneList; focus_ = -1; return true; }
                    state_->mode = Mode::TREE;
                    if (state_->focus_treeview) state_->focus_treeview();
                    return true;
                }
                if (event == ftxui::Event::ArrowLeft || event == ftxui::Event::ArrowRight)
                {
                    if (which_ == ZoneForm)
                    {
                        tab_ = (tab_ + (event == ftxui::Event::ArrowRight ? 1 : 4)) % 5;
                        focus_ = -1;
                    }
                    return true;
                }
                if (event == ftxui::Event::Tab)
                {
                    CycleZone(+1);
                    return true;
                }
                if (event == ftxui::Event::ArrowDown)
                {
                    if (which_ == ZoneList) MoveList(+1);
                    else FocusNext();
                    return true;
                }
                if (event == ftxui::Event::ArrowUp)
                {
                    if (which_ == ZoneList) MoveList(-1);
                    else FocusPrev();
                    return true;
                }
                if (event == ftxui::Event::Return)
                {
                    ActivateZone();
                    return true;
                }
                if (event == ftxui::Event::Backspace)
                {
                    if (which_ == ZoneList && !search_.empty()) { search_.pop_back(); return true; }
                    return true;
                }
                if (event.is_character() && !event.character().empty())
                {
                    HandleCharacter(event.character());
                    return true;
                }
                return false;
            }

            ftxui::Element Render() override
            {
                Rebuild();
                if (confirming_delete_) return RenderConfirm();
                if (dropdown_open_) return RenderDropdownOverlay();
                return RenderPane();
            }

        private:
            std::shared_ptr<EditorState> state_;
            int which_ = ZoneList;
            int focus_ = -1;        // list index or widget index / action index
            int list_sel_ = -1;
            int tab_ = 0;
            std::string search_;
            size_t list_top_ = 0;
            std::vector<size_t> visible_;
            std::vector<Widget> widgets_;
            Player draft_;
            bool is_new_ = false;
            std::string status_;

            bool dropdown_open_ = false;
            int dropdown_sel_ = 0;
            std::string dropdown_title_;
            std::vector<std::string> dropdown_opts_;
            Widget* dropdown_widget_ = nullptr;

            bool confirming_delete_ = false;

            // ---- Editing target ----

            std::optional<Player*> Editing()
            {
                if (is_new_) return &draft_;
                if (list_sel_ >= 0 && static_cast<size_t>(list_sel_) < state_->players.size())
                    return &state_->players[static_cast<size_t>(list_sel_)];
                return std::nullopt;
            }

            Player* EditingOrNull()
            {
                auto e = Editing();
                return e ? *e : nullptr;
            }

            // ---- Focus zones ----

            void CycleZone(int dir)
            {
                which_ = (which_ + dir + ZoneCount) % ZoneCount;
                focus_ = -1;
                EndTyping();
            }

            void FocusNext()
            {
                if (widgets_.empty()) { focus_ = -1; return; }
                focus_ = (focus_ + 1) % static_cast<int>(widgets_.size());
                EndTyping();
            }

            void FocusPrev()
            {
                if (widgets_.empty()) { focus_ = -1; return; }
                focus_ = (focus_ - 1 + static_cast<int>(widgets_.size()))
                         % static_cast<int>(widgets_.size());
                EndTyping();
            }

            void MoveList(int dir)
            {
                if (visible_.empty()) { list_sel_ = -1; return; }
                int cur = 0;
                if (list_sel_ >= 0)
                {
                    auto it = std::find(visible_.begin(), visible_.end(),
                                        static_cast<size_t>(list_sel_));
                    if (it != visible_.end()) cur = static_cast<int>(std::distance(visible_.begin(), it));
                }
                cur = std::max(0, std::min(static_cast<int>(visible_.size()) - 1, cur + dir));
                is_new_ = false;
                list_sel_ = static_cast<int>(visible_[static_cast<size_t>(cur)]);
                tab_ = 0;
                EndTyping();
            }

            void ActivateZone()
            {
                if (which_ == ZoneForm)
                {
                    if (widgets_.empty()) return;
                    Widget& w = widgets_[static_cast<size_t>(focus_ < 0 ? 0 : focus_)];
                    if (w.kind == Kind::Button) { if (w.on) w.on(); return; }
                    if (w.kind == Kind::Dropdown) { OpenDropdown(&w); return; }
                    if (w.kind == Kind::Checkbox) { if (w.toggle) w.toggle(); return; }
                    return;
                }
                if (which_ == ZoneList)
                {
                    // Entering on a list item already selects it; nothing more to do.
                    return;
                }
            }

            // ---- Action bar ----

            static constexpr int kNumActions = 5;
            // order: New, Save, Delete, Generate, Random

            Element RenderActionBar()
            {
                const char* labels[kNumActions] = { "New", "Save", "Delete", "Generate", "Random" };
                Elements parts;
                for (int i = 0; i < kNumActions; ++i)
                {
                    std::string l = " [" + std::string(labels[i]) + "] ";
                    bool focused = (which_ == ZoneActions) && (focus_ == i);
                    parts.push_back(text(l) | ftxui::bold
                                    | (focused ? ftxui::inverted : ftxui::nothing));
                }
                return ftxui::hbox(std::move(parts));
            }

            Element RenderTabs()
            {
                const char* names[5] = { "Identity", "Abilities", "Skills", "Saves", "Details" };
                Elements parts;
                for (int i = 0; i < 5; ++i)
                {
                    if (i == tab_) parts.push_back(text("<" + std::string(names[i]) + ">") | ftxui::inverted | ftxui::bold);
                    else parts.push_back(text(" " + std::string(names[i]) + " ") | ftxui::bold);
                }
                return ftxui::hbox(std::move(parts));
            }

            bool OnActionZoneEvent(ftxui::Event event)
            {
                // Tab leaves the action zone (it is one of the focus zones).
                if (event == ftxui::Event::Tab)
                {
                    CycleZone(+1);
                    return true;
                }
                if (event == ftxui::Event::TabReverse)
                {
                    CycleZone(-1);
                    return true;
                }
                if (event == ftxui::Event::ArrowRight)
                {
                    focus_ = (std::max(0, focus_) + 1) % kNumActions;
                    return true;
                }
                if (event == ftxui::Event::ArrowLeft)
                {
                    focus_ = (std::max(0, focus_) - 1 + kNumActions) % kNumActions;
                    return true;
                }
                if (event == ftxui::Event::Return)
                {
                    ActivateAction(std::max(0, focus_));
                    return true;
                }
                if (event == ftxui::Event::Escape)
                {
                    which_ = ZoneList;
                    focus_ = -1;
                    return true;
                }
                if (event.is_character() && event.character().size() == 1
                    && event.character()[0] >= '1' && event.character()[0] <= '5')
                {
                    ActivateAction(event.character()[0] - '1');
                    return true;
                }
                return true;
            }

            void ActivateAction(int i)
            {
                switch (i)
                {
                    case 0: StartNew(); break;
                    case 1: Save(); break;
                    case 2: confirming_delete_ = true; break;
                    case 3: case 4: Generate(); break;
                }
            }

            void StartNew()
            {
                draft_ = Player{};
                draft_.speed = "30 ft.";
                is_new_ = true;
                list_sel_ = -1;
                which_ = ZoneForm;
                focus_ = -1;
                tab_ = 0;
                status_.clear();
            }

            void SelectList(size_t i)
            {
                is_new_ = false;
                list_sel_ = static_cast<int>(i);
                which_ = ZoneForm;
                focus_ = -1;
                tab_ = 0;
                status_.clear();
            }

            void Save()
            {
                Player* p = EditingOrNull();
                if (!p)
                {
                    if (is_new_) state_->players.push_back(draft_);
                    return;
                }
                if (p->name.empty()) p->name = "Unnamed Character";
                if (is_new_)
                {
                    state_->players.push_back(*p);
                    is_new_ = false;
                    list_sel_ = static_cast<int>(state_->players.size()) - 1;
                }
                else
                {
                    state_->players[static_cast<size_t>(list_sel_)] = *p;
                }
                state_->changed = true;
                status_ = "Saved " + p->name;
            }

            void DoDelete()
            {
                if (is_new_) { StartNew(); return; }
                if (list_sel_ < 0 || static_cast<size_t>(list_sel_) >= state_->players.size()) return;
                state_->players.erase(state_->players.begin() + list_sel_);
                state_->changed = true;
                list_sel_ = std::min(list_sel_, static_cast<int>(state_->players.size()) - 1);
                if (list_sel_ < 0) StartNew();
                tab_ = 0;
            }

            void Generate()
            {
                Player* p = EditingOrNull();
                if (!p || !state_->dnd) return;
                p->name = RandomName();
                if (!state_->dnd->races.empty())
                {
                    auto it = state_->dnd->races.begin();
                    std::advance(it, Random::get(0, static_cast<int>(state_->dnd->races.size()) - 1));
                    p->race = it->first;
                }
                if (!state_->dnd->classes.empty())
                {
                    auto it = state_->dnd->classes.begin();
                    std::advance(it, Random::get(0, static_cast<int>(state_->dnd->classes.size()) - 1));
                    p->char_class = it->first;
                }
                if (!state_->dnd->alignments.empty())
                    p->alignment = state_->dnd->alignments[Random::get(0, static_cast<int>(state_->dnd->alignments.size()) - 1)];
                if (!state_->dnd->backgrounds.empty())
                {
                    auto it = state_->dnd->backgrounds.begin();
                    std::advance(it, Random::get(0, static_cast<int>(state_->dnd->backgrounds.size()) - 1));
                    p->background = it->first;
                }
                p->level = Random::get(1, 20);
                ApplyRace(*p, true);
                RollAbilities();
                ApplyClassSaves();
                RefreshEquipment();
                RefreshFeatures();
                status_ = "Generated " + p->name;
            }

            // Roll the character's traits from what is already set in the
            // Identity tab (race / class / background / level are kept, the
            // attributes and derived numbers are rolled to match them).
            void RollIdentity()
            {
                Player* p = EditingOrNull();
                if (!p || !state_->dnd) return;
                p->str = Roll4d6DropLowest();
                p->dex = Roll4d6DropLowest();
                p->con = Roll4d6DropLowest();
                p->intel = Roll4d6DropLowest();
                p->wis = Roll4d6DropLowest();
                p->cha = Roll4d6DropLowest();
                auto rit = state_->dnd->races.find(p->race);
                if (rit != state_->dnd->races.end())
                {
                    for (const auto& kv : rit->second.ability)
                    {
                        if (kv.first == "STR") p->str += kv.second;
                        else if (kv.first == "DEX") p->dex += kv.second;
                        else if (kv.first == "CON") p->con += kv.second;
                        else if (kv.first == "INT") p->intel += kv.second;
                        else if (kv.first == "WIS") p->wis += kv.second;
                        else if (kv.first == "CHA") p->cha += kv.second;
                    }
                    p->speed = std::to_string(rit->second.speed) + " ft.";
                }
                ApplyClass(*p);
                RecomputeDerived(*p);
                ApplyClassSaves();
                RefreshEquipment();
                RefreshFeatures();
                status_ = "Rolled from Identity";
            }

            // ---- Confidence / confirm ----

            bool OnConfirmEvent(ftxui::Event event)
            {
                if (event.is_character() && (event.character() == "y" || event.character() == "Y"))
                {
                    DoDelete();
                    confirming_delete_ = false;
                    return true;
                }
                if (event == ftxui::Event::Escape
                    || (event.is_character() && (event.character() == "n" || event.character() == "N")))
                {
                    confirming_delete_ = false;
                    return true;
                }
                return true;
            }

            Element RenderConfirm()
            {
                Player* p = EditingOrNull();
                const std::string who = (p && !p->name.empty()) ? p->name : "this player";
                return ftxui::window(text(" Delete Player "),
                    ftxui::vbox({
                        text("  Delete " + who + "?  (y = yes, n/Esc = no)  "),
                    })) | ftxui::center;
            }

            // ---- Input editing ----

            std::optional<bool> typing_;
            std::string edit_buffer_;

            void EndTyping()
            {
                typing_.reset();
                edit_buffer_.clear();
            }

            void HandleCharacter(const std::string& ch)
            {
                if (which_ == ZoneList)
                {
                    search_ += ch;
                    return;
                }
                if (which_ == ZoneForm)
                {
                    Player* p = EditingOrNull();
                    if (!p) return;
                    if (widgets_.empty()) return;
                    Widget& w = widgets_[static_cast<size_t>(std::max(0, focus_))];
                    if (w.kind == Kind::Number)
                    {
                        if (!typing_.has_value()) { edit_buffer_ = ""; typing_ = true; }
                        if (!edit_buffer_.empty() || ch != "0") edit_buffer_ += ch;
                        int v = 0; try { v = std::stoi(edit_buffer_); } catch (...) {}
                        if (w.number) { *w.number = v; if (w.on_change) w.on_change(); }
                    }
                    else if (w.kind == Kind::Input)
                    {
                        if (!typing_.has_value()) { *w.text = ""; typing_ = true; }
                        *w.text += ch;
                    }
                    return;
                }
            }

            // ---- Dropdown ----

            void OpenDropdown(Widget* w)
            {
                dropdown_widget_ = w;
                dropdown_title_ = w->label;
                dropdown_opts_ = w->options;
                int cur = -1;
                if (w->text)
                {
                    for (size_t i = 0; i < w->options.size(); ++i)
                        if (w->options[i] == *w->text) { cur = static_cast<int>(i); break; }
                }
                dropdown_sel_ = std::max(0, cur);
                dropdown_open_ = true;
            }

            bool OnDropdownEvent(ftxui::Event event)
            {
                if (event == ftxui::Event::Escape)
                {
                    dropdown_open_ = false;
                    return true;
                }
                if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Tab)
                {
                    dropdown_sel_ = std::min(dropdown_sel_ + 1, static_cast<int>(dropdown_opts_.size()) - 1);
                    return true;
                }
                if (event == ftxui::Event::ArrowUp || event == ftxui::Event::TabReverse)
                {
                    dropdown_sel_ = std::max(0, dropdown_sel_ - 1);
                    return true;
                }
                if (event == ftxui::Event::Return)
                {
                    if (dropdown_widget_ && dropdown_widget_->text)
                        *dropdown_widget_->text = dropdown_opts_[static_cast<size_t>(dropdown_sel_)];
                    if (dropdown_widget_ && dropdown_widget_->on_change)
                        dropdown_widget_->on_change();
                    dropdown_open_ = false;
                    return true;
                }
                if (event.is_character() && !event.character().empty())
                {
                    const char c = Lower(event.character())[0];
                    for (size_t i = 0; i < dropdown_opts_.size(); ++i)
                    {
                        std::string o = Lower(dropdown_opts_[i]);
                        if (!o.empty() && o[0] == c) { dropdown_sel_ = static_cast<int>(i); return true; }
                    }
                    return true;
                }
                return true;
            }

            Element RenderDropdownOverlay()
            {
                const int n = static_cast<int>(dropdown_opts_.size());
                const int win = 10;                 // visible options per page
                int top = std::max(0, dropdown_sel_ - win + 1);
                ftxui::Elements items;
                for (int k = top; k < n && k < top + win; ++k)
                {
                    std::string l = " " + PadTo(dropdown_opts_[static_cast<size_t>(k)], 22) + " ";
                    bool sel = (k == dropdown_sel_);
                    items.push_back(text(l) | (sel ? ftxui::inverted
                                                   : ftxui::nothing));
                }
                ftxui::Elements col;
                col.push_back(ftxui::vbox(std::move(items)) | ftxui::frame | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, static_cast<int>(std::min(win, n))));
                col.push_back(ftxui::separator());
                col.push_back(text("  Enter: select   Up/Down: move   Esc: cancel  ") | ftxui::dim);
                std::string title = " " + dropdown_title_ + " ";
                return ftxui::window(text(title), ftxui::vbox(std::move(col))) | ftxui::center;
            }

            // ---- Rebuild widgets ----

            void Rebuild()
            {
                if (list_sel_ >= static_cast<int>(state_->players.size()))
                    list_sel_ = static_cast<int>(state_->players.size()) - 1;
                if (list_sel_ < 0 && !state_->players.empty() && !is_new_)
                    list_sel_ = 0;
                visible_ = Filtered();

                widgets_.clear();
                Player* p = EditingOrNull();
                if (p)
                {
                    switch (tab_)
                    {
                        case 0: BuildIdentity(*p); break;
                        case 1: BuildAbilities(*p); break;
                        case 2: BuildSkills(*p); break;
                        case 3: BuildSaves(*p); break;
                        default: BuildDetails(*p); break;
                    }
                }
                if (widgets_.empty()) focus_ = -1;
                else focus_ = std::max(-1, std::min(focus_, static_cast<int>(widgets_.size()) - 1));
            }

            std::vector<size_t> Filtered() const
            {
                std::vector<size_t> v;
                for (size_t i = 0; i < state_->players.size(); ++i)
                {
                    if (search_.empty()
                        || Lower(state_->players[i].name).find(Lower(search_)) != std::string::npos)
                        v.push_back(i);
                }
                return v;
            }

            // ---- Derived computations ----

            int ModFor(Player& p) const { (void)p; return 0; }

            void ApplyRace(Player& p, bool force)
            {
                if (!state_->dnd) return;
                auto it = state_->dnd->races.find(p.race);
                if (it != state_->dnd->races.end()) p.speed = std::to_string(it->second.speed) + " ft.";
                (void)force;
            }

            void ApplyClass(Player& p)
            {
                if (!state_->dnd) return;
                auto it = state_->dnd->classes.find(p.char_class);
                if (it != state_->dnd->classes.end())
                    p.hit_dice = std::to_string(std::max(1, p.level)) + "d" + std::to_string(it->second.hit_die);
            }

            void ApplyBackground(Player& p)
            {
                if (!state_->dnd) return;
                auto it = state_->dnd->backgrounds.find(p.background);
                if (it != state_->dnd->backgrounds.end() && p.equipment.empty())
                    p.equipment = Join(it->second, ", ");
            }

            void ApplyClassSaves()
            {
                Player* p = EditingOrNull();
                if (!p || !state_->dnd) return;
                auto it = state_->dnd->classes.find(p->char_class);
                if (it != state_->dnd->classes.end()) p->proficient_saves = it->second.saves;
            }

            void RollAbilities()
            {
                Player* p = EditingOrNull();
                if (!p) return;
                p->str = Roll4d6DropLowest();
                p->dex = Roll4d6DropLowest();
                p->con = Roll4d6DropLowest();
                p->intel = Roll4d6DropLowest();
                p->wis = Roll4d6DropLowest();
                p->cha = Roll4d6DropLowest();
                RecomputeDerived(*p);
            }

            void RecomputeDerived(Player& p)
            {
                if (!state_->dnd) return;
                auto cit = state_->dnd->classes.find(p.char_class);
                const int con_mod = AbilityMod(p.con);
                if (cit != state_->dnd->classes.end())
                {
                    p.hp = RollHitPoints(cit->second.hit_die, std::max(1, p.level), con_mod);
                    p.hit_dice = std::to_string(std::max(1, p.level)) + "d" + std::to_string(cit->second.hit_die);
                }
                const int dex_mod = AbilityMod(p.dex);
                int ac = 10 + dex_mod;
                if (p.char_class == "Monk") ac = 10 + dex_mod + AbilityMod(p.wis);
                else if (p.char_class == "Barbarian") ac = 10 + dex_mod + con_mod;
                p.ac = ac;
            }

            void RefreshEquipment()
            {
                Player* p = EditingOrNull();
                if (!p || !state_->dnd) return;
                std::vector<std::string> equips;
                auto cit = state_->dnd->classes.find(p->char_class);
                if (cit != state_->dnd->classes.end())
                    for (const auto& e : cit->second.equipment) equips.push_back(e);
                auto bit = state_->dnd->backgrounds.find(p->background);
                if (bit != state_->dnd->backgrounds.end())
                    for (const auto& e : bit->second) equips.push_back(e);
                equips.push_back("15 GP");
                equips.push_back("Common Clothes");
                p->equipment = Join(equips, ", ");
            }

            void RefreshFeatures()
            {
                Player* p = EditingOrNull();
                if (!p || !state_->dnd) return;
                std::vector<std::string> feats;
                auto rit = state_->dnd->races.find(p->race);
                if (rit != state_->dnd->races.end())
                    for (const auto& f : rit->second.features) feats.push_back(f);
                auto cit = state_->dnd->classes.find(p->char_class);
                if (cit != state_->dnd->classes.end())
                    for (const ClassFeature& f : cit->second.features)
                        if (f.level <= p->level)
                            feats.push_back("Level " + std::to_string(f.level) + ": " + f.name + " - " + f.desc);
                p->features = Join(feats, ", ");
            }

            // ---- Option lists ----

            std::vector<std::string> RaceOptions() const
            {
                std::vector<std::string> v;
                if (state_->dnd) for (const auto& kv : state_->dnd->races) v.push_back(kv.first);
                return v;
            }
            std::vector<std::string> ClassOptions() const
            {
                std::vector<std::string> v;
                if (state_->dnd) for (const auto& kv : state_->dnd->classes) v.push_back(kv.first);
                return v;
            }
            std::vector<std::string> BackgroundOptions() const
            {
                std::vector<std::string> v;
                if (state_->dnd) for (const auto& kv : state_->dnd->backgrounds) v.push_back(kv.first);
                return v;
            }
            std::vector<std::string> AlignmentOptions() const
            {
                std::vector<std::string> v;
                if (state_->dnd) return state_->dnd->alignments;
                return v;
            }

            // ---- Tab builders ----

            void BuildIdentity(Player& p)
            {
                if (state_->dnd)
                {
                    if (state_->dnd->races.count(p.race) == 0 && !state_->dnd->races.empty())
                        p.race = state_->dnd->races.begin()->first;
                    if (state_->dnd->classes.count(p.char_class) == 0 && !state_->dnd->classes.empty())
                        p.char_class = state_->dnd->classes.begin()->first;
                    if (state_->dnd->backgrounds.count(p.background) == 0 && !state_->dnd->backgrounds.empty())
                        p.background = state_->dnd->backgrounds.begin()->first;
                }

                Widget name; name.kind = Kind::Input; name.label = "Name"; name.text = &p.name;
                widgets_.push_back(name);

                Widget race; race.kind = Kind::Dropdown; race.label = "Race"; race.text = &p.race;
                race.options = RaceOptions();
                race.on_change = [this]{ if (auto p = EditingOrNull()) ApplyRace(*p, false); };
                widgets_.push_back(race);

                Widget cls; cls.kind = Kind::Dropdown; cls.label = "Class"; cls.text = &p.char_class;
                cls.options = ClassOptions();
                cls.on_change = [this]{ if (auto p = EditingOrNull()) ApplyClass(*p); };
                widgets_.push_back(cls);

                Widget bkg; bkg.kind = Kind::Dropdown; bkg.label = "Background"; bkg.text = &p.background;
                bkg.options = BackgroundOptions();
                bkg.on_change = [this]{ if (auto p = EditingOrNull()) ApplyBackground(*p); };
                widgets_.push_back(bkg);

                Widget aln; aln.kind = Kind::Dropdown; aln.label = "Alignment"; aln.text = &p.alignment;
                aln.options = AlignmentOptions();
                widgets_.push_back(aln);

                Widget lvl; lvl.kind = Kind::Number; lvl.label = "Level"; lvl.number = &p.level;
                lvl.on_change = [this]{ if (auto p = EditingOrNull()) ApplyClass(*p); };
                widgets_.push_back(lvl);

                Widget ac; ac.kind = Kind::Number; ac.label = "AC"; ac.number = &p.ac; widgets_.push_back(ac);
                Widget hp; hp.kind = Kind::Number; hp.label = "HP"; hp.number = &p.hp; widgets_.push_back(hp);

                Widget spd; spd.kind = Kind::Input; spd.label = "Speed"; spd.text = &p.speed; widgets_.push_back(spd);

                // A "Generate" section: roll the attributes / derived numbers
                // from the Identity settings chosen above.
                Widget gen; gen.kind = Kind::Button; gen.label = "Generate traits";
                gen.on = [this]{ RollIdentity(); };
                widgets_.push_back(gen);
            }

            void BuildAbilities(Player& p)
            {
                int* vals[6] = { &p.str, &p.dex, &p.con, &p.intel, &p.wis, &p.cha };
                for (int i = 0; i < 6; ++i)
                {
                    Widget w; w.kind = Kind::Number; w.label = kAbilityNames[i]; w.number = vals[i];
                    w.on_change = [this]{ if (auto p = EditingOrNull()) RecomputeDerived(*p); };
                    w.suffix = [this, number = vals[i]]{
                        return "(" + FormatMod(AbilityMod(*number)) + ")";
                    };
                    widgets_.push_back(w);
                }
                Widget roll; roll.kind = Kind::Button; roll.label = "Roll 4d6";
                roll.on = [this]{ RollAbilities(); };
                widgets_.push_back(roll);
            }

            void BuildSkills(Player& p)
            {
                if (!state_->dnd) return;
                for (const auto& kv : state_->dnd->skills)
                {
                    const std::string& skill = kv.first;
                    Widget w; w.kind = Kind::Checkbox; w.label = skill; w.hint = kv.second;
                    w.checked = [this, skill]{
                        auto p = EditingOrNull();
                        return p && std::find(p->proficient_skills.begin(), p->proficient_skills.end(), skill)
                                    != p->proficient_skills.end();
                    };
                    w.toggle = [this, skill]{
                        auto p = EditingOrNull();
                        if (!p) return;
                        auto it = std::find(p->proficient_skills.begin(), p->proficient_skills.end(), skill);
                        if (it != p->proficient_skills.end()) p->proficient_skills.erase(it);
                        else p->proficient_skills.push_back(skill);
                    };
                    widgets_.push_back(w);
                }
            }

            void BuildSaves(Player& p)
            {
                for (int i = 0; i < 6; ++i)
                {
                    const std::string stat = kAbilityNames[i];
                    Widget w; w.kind = Kind::Checkbox; w.label = stat;
                    w.checked = [this, stat]{
                        auto p = EditingOrNull();
                        return p && std::find(p->proficient_saves.begin(), p->proficient_saves.end(), stat)
                                    != p->proficient_saves.end();
                    };
                    w.toggle = [this, stat]{
                        auto p = EditingOrNull();
                        if (!p) return;
                        auto it = std::find(p->proficient_saves.begin(), p->proficient_saves.end(), stat);
                        if (it != p->proficient_saves.end()) p->proficient_saves.erase(it);
                        else p->proficient_saves.push_back(stat);
                    };
                    widgets_.push_back(w);
                }
                Widget cls; cls.kind = Kind::Button; cls.label = "Apply class saves";
                cls.on = [this]{ ApplyClassSaves(); };
                widgets_.push_back(cls);
            }

            void BuildDetails(Player& p)
            {
                Widget acb; acb.kind = Kind::Input; acb.label = "AC Bonus"; acb.text = &p.ac_bonus; widgets_.push_back(acb);
                Widget hd; hd.kind = Kind::Input; hd.label = "Hit Dice"; hd.text = &p.hit_dice; widgets_.push_back(hd);
                Widget eq; eq.kind = Kind::Input; eq.label = "Equipment"; eq.text = &p.equipment; widgets_.push_back(eq);
                Widget ft; ft.kind = Kind::Input; ft.label = "Features"; ft.text = &p.features; widgets_.push_back(ft);

                Widget re; re.kind = Kind::Button; re.label = "Refresh equipment";
                re.on = [this]{ RefreshEquipment(); };
                widgets_.push_back(re);

                Widget rf; rf.kind = Kind::Button; rf.label = "Refresh features";
                rf.on = [this]{ RefreshFeatures(); };
                widgets_.push_back(rf);
            }

            // ---- Rendering ----

            Element RenderWidget(const Widget& w, bool focused)
            {
                switch (w.kind)
                {
                    case Kind::Button:
                    {
                        std::string l = " [ " + w.label + " ] ";
                        Element e = text(l) | ftxui::bold;
                        if (focused) e = e | ftxui::inverted;
                        return e;
                    }
                    case Kind::Input:
                    {
                        std::string val = w.text ? *w.text : "";
                        std::string line = "  " + PadTo(w.label, 12) + ": " + val;
                        if (focused && typing_.has_value()) line += "_";
                        Element e = text(line);
                        if (focused) e = e | ftxui::inverted;
                        return e;
                    }
                    case Kind::Number:
                    {
                        std::string val = w.number ? std::to_string(*w.number) : "0";
                        std::string line = "  " + PadTo(w.label, 12) + ": " + val;
                        if (w.suffix) line += "  " + w.suffix();
                        if (focused && typing_.has_value()) line += "_";
                        Element e = text(line);
                        if (focused) e = e | ftxui::inverted;
                        return e;
                    }
                    case Kind::Dropdown:
                    {
                        std::string cur = w.text ? *w.text : "";
                        std::string line = "  " + PadTo(w.label, 12) + ": " + cur + "  [v]";
                        Element e = text(line);
                        if (focused) e = e | ftxui::inverted;
                        return e;
                    }
                    case Kind::Checkbox:
                    {
                        const bool on = w.checked ? w.checked() : false;
                        std::string line = "  " + std::string(on ? "[x]" : "[ ]") + " " + w.label;
                        if (!w.hint.empty()) line += " (" + w.hint + ")";
                        Element e = text(line);
                        if (focused) e = e | ftxui::inverted;
                        return e;
                    }
                }
                return text("");
            }

            Element RenderList()
            {
                Elements rows;
                rows.push_back(text(" Saved Characters ") | ftxui::bold);
                rows.push_back(text(" / " + search_ + "_ ") | ftxui::dim);
                rows.push_back(ftxui::separator());
                if (visible_.empty())
                {
                    rows.push_back(text("  (none)") | ftxui::dim);
                }
                else
                {
                    const size_t win = 11;
                    size_t top = list_top_;
                    if (list_sel_ >= 0)
                    {
                        auto it = std::find(visible_.begin(), visible_.end(), static_cast<size_t>(list_sel_));
                        if (it != visible_.end())
                        {
                            size_t idx = static_cast<size_t>(std::distance(visible_.begin(), it));
                            if (idx < top) top = idx;
                            if (idx >= top + win) top = idx - win + 1;
                        }
                    }
                    for (size_t k = top; k < visible_.size() && k < top + win; ++k)
                    {
                        const size_t real = visible_[k];
                        const Player& pl = state_->players[real];
                        std::string line = "  " + PadTo(pl.name.empty() ? "(unnamed)" : pl.name, 16)
                                           + " L" + std::to_string(pl.level) + " " + pl.race;
                        if (line.size() > 26) line = line.substr(0, 26);
                        bool sel = (static_cast<int>(real) == list_sel_);
                        Element e = text(line);
                        if (sel) e = e | ftxui::inverted;
                        rows.push_back(e);
                    }
                }
                rows.push_back(ftxui::separator());
                return ftxui::vbox(std::move(rows)) | ftxui::frame;
            }

            Element RenderEditor()
            {
                Player* p = EditingOrNull();
                if (!p)
                {
                    Elements e;
                    e.push_back(text("  No player selected.") | ftxui::dim);
                    e.push_back(text("  Tab to the [New] button and press Enter.") | ftxui::dim);
                    e.push_back(ftxui::emptyElement() | ftxui::flex);
                    return ftxui::vbox(std::move(e));
                }

                Elements rows;
                rows.push_back(RenderTabs());
                rows.push_back(ftxui::separator());
                rows.push_back(ftxui::hbox({
                    text("  " + p->name + "  ") | ftxui::bold,
                    text("Lvl " + std::to_string(p->level) + " " + p->race + " " + p->char_class) | ftxui::dim,
                }));
                rows.push_back(ftxui::separator());

                Elements fields;
                for (size_t i = 0; i < widgets_.size(); ++i)
                {
                    bool focused = (which_ == ZoneForm) && (static_cast<int>(i) == focus_);
                    fields.push_back(RenderWidget(widgets_[i], focused));
                }
                rows.push_back(ftxui::vbox(std::move(fields)) | ftxui::frame | ftxui::flex);

                rows.push_back(ftxui::separator());
                std::string hint = "  Tab: zones  <-/->: tab  Enter: activate  Esc: exit  ";
                if (!status_.empty()) hint = "  " + status_;
                rows.push_back(text(hint) | ftxui::dim);
                return ftxui::vbox(std::move(rows)) | ftxui::flex;
            }

            Element RenderPane()
            {
                Elements rows;
                rows.push_back(RenderActionBar());
                rows.push_back(ftxui::separator());
                rows.push_back(ftxui::hbox({
                    RenderList() | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 28),
                    ftxui::separator(),
                    RenderEditor() | ftxui::flex,
                }) | ftxui::flex);
                return ftxui::vbox(std::move(rows));
            }
        };
    }

    ftxui::Component MakePlayers(std::shared_ptr<EditorState> state)
    {
        return ftxui::Make<PlayersPane>(std::move(state));
    }
}
