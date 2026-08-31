#pragma once

#include <memory>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace terminadventure::history
{
    // Record `id` as viewed (most recent last, deduplicated, capped at
    // EditorState::kHistoryMax). Called by the treeview when a node with
    // text is selected and by the editor whenever it saves non-empty text.
    void Record(EditorState& state, const std::string& id);

    // Build the history dialog. While *show is true it consumes every
    // event, so no app key bindings fire. j/k (and ArrowUp/ArrowDown) move
    // the selection, Enter jumps to the selected node (revealing it in the
    // tree, staying in the current mode), Escape cancels. The list shows the
    // most recently viewed node first. Nodes that no longer exist are listed
    // as "(deleted node)" and can be skipped with Enter.
    ftxui::Component MakeHistoryDialog(std::shared_ptr<EditorState> state, bool* show);
}
