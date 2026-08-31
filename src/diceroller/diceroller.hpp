#pragma once

#include <memory>
#include <ftxui/component/component.hpp>
#include "../editor/editor_state.hpp"

namespace terminadventure::diceroller
{
    // A dedicated dice-roller UI for ROLLER-type nodes: an output area above
    // an input field. Typing a roll (e.g. "2d6", "(2)4d6", "2*3d6+2") and
    // pressing Enter appends the result to the output.
    ftxui::Component MakeDiceRoller(std::shared_ptr<EditorState> state);
}
