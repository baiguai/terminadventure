#pragma once

#include <memory>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace terminadventure::bookmarks
{
    // Build the bookmarks dialog. While *show is true it consumes every
    // event, so no app key bindings fire. j/k (and ArrowUp/ArrowDown) move
    // the selection, Enter jumps to the selected bookmark (revealing the
    // node in the tree and, for line bookmarks, moving the editor cursor to
    // that line), Escape cancels. Bookmarked nodes that no longer exist are
    // listed as "(deleted node)" and can be skipped with Enter.
    ftxui::Component MakeBookmarksDialog(std::shared_ptr<EditorState> state, bool* show);
}
