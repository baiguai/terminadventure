#pragma once

#include <memory>
#include <string>

#include <ftxui/component/component.hpp>

#include "../editor/editor_state.hpp"
#include "dnd_data.hpp"

namespace terminadventure::players
{
    // Loads ./config/dnd.conf (or the given path). Returns nullptr on failure.
    std::shared_ptr<DnDData> LoadDnDData(const std::string& path);

    // Derives the modifier for an ability score: floor((score - 10) / 2).
    int AbilityMod(int score);
    // The proficiency bonus for a level: ceil(1 + level / 4).
    int ProficiencyBonus(int level);
    // Formats a modifier with an explicit sign, e.g. +3 / -2.
    std::string FormatMod(int mod);

    ftxui::Component MakePlayers(std::shared_ptr<EditorState> state);
}
