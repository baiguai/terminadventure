#pragma once

#include <memory>
#include <vector>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace terminadventure::treeview
{
    struct TreeNode;
}

namespace terminadventure::search
{
    // Find matches for `raw_query` inside the currently active node only
    // (the same prefix rules as the search dialog apply: a leading "r:" makes
    // it a case-insensitive regex, a leading ":" restricts it to node titles,
    // otherwise it is a case-insensitive substring match). Returns either the
    // active node (when it matches) or nothing, so the Vim-style '/' find and
    // n/N navigation stay confined to the node being edited.
    std::vector<terminadventure::treeview::TreeNode*> FindMatches(
        std::shared_ptr<EditorState> state, const std::string& raw_query);

    // Ranges [lo, hi) of every occurrence of `raw_query` inside `line`, using
    // the same prefix rules as FindMatches (a leading "r:" makes it a
    // case-insensitive regex, a leading ":" restricts it to titles, so a
    // title-only query never matches content). Empty for no matches, an empty
    // query, or an invalid regex. Used to highlight matches in the editor text.
    std::vector<std::pair<int, int>> FindLineMatches(const std::string& line,
                                                     const std::string& raw_query);

    // Build the node search/filter dialog. While *show is true it consumes
    // every event, so no app key bindings fire. ArrowUp/ArrowDown move the
    // selection, Enter jumps to the selected node (revealing it in the tree),
    // Escape cancels. A leading "r:" makes the query a case-insensitive regex;
    // otherwise it is a case-insensitive substring match against node titles
    // and content. The dialog keeps a fixed size regardless of result count.
    // With `insert_mode` set, Enter instead inserts a `_Title_` node link to
    // the selected node into the currently active node at the editor cursor
    // (via state->insert_text_at_cursor) and closes.
    ftxui::Component MakeSearchDialog(std::shared_ptr<EditorState> state, bool* show,
                                      bool insert_mode = false);
}
