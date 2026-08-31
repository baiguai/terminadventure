#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

#include "../editor/editor_state.hpp"
#include "../treeview/tree_node.hpp"

namespace terminadventure::undo
{
    // First text line of `node` (node titles excluded), used as the label for
    // the currently active note's undo entries.
    std::string FirstTextLine(const treeview::TreeNode& node);

    ftxui::Component MakeUndoDialog(std::shared_ptr<EditorState> state, bool* show);
}
