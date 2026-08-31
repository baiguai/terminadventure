#pragma once

#include <memory>
#include <ftxui/component/component.hpp>
#include "../editor/editor_state.hpp"

namespace terminadventure::rightpane
{
    // The content pane to the right of the tree. It dispatches to a dedicated
    // UI based on the active node's type: ROLLER nodes show the dice roller;
    // every other (editable) node shows the classic text editor. Whether a
    // node type is editable is handled by the treeview's protection rules.
    ftxui::Component MakeRightPane(std::shared_ptr<EditorState> state);
}
