#pragma once

#include <functional>
#include <string>
#include <map>
#include <utility>
#include <vector>
#include "../bookmark/bookmark.hpp"
#include "../mode/mode.hpp"
#include "../keyboard/keymap.hpp"
#include "../op/op.hpp"
#include "../treeview/tree_node.hpp"

inline constexpr int kDefaultTreeviewWidth = 30;
inline constexpr int kMinTreeviewWidth = 10;
inline constexpr int kMaxTreeviewWidth = 200;

// A single undoable point-in-time snapshot of the document. `json` is the
// full serialized state (tree + bookmarks + history + width); `preview` is
// a short human label for the undo dialog's list.
struct UndoState {
    std::string json;
    std::string preview;
};

struct EditorState {
    Mode mode = Mode::TREE;
    std::string command_buffer;
    int command_cursor = 0;
    int* active_child = nullptr;
    Mode mode_before_command = Mode::TREE;
    bool changed = false;

    int treeview_width = kDefaultTreeviewWidth;

    terminadventure::treeview::TreeNode* active_node = nullptr;
    std::string status;
    std::string template_path;
    std::string init_path;
    std::vector<terminadventure::bookmark::Bookmark> bookmarks;

    // Viewed-node history: node ids of every selected node that has text,
    // most recent last, deduplicated, capped at kHistoryMax. Populated by
    // the treeview; the history dialog resolves ids to live nodes.
    std::vector<std::string> history;
    static constexpr std::size_t kHistoryMax = 30;

    std::vector<std::string> recent_files;
    static constexpr std::size_t kRecentMax = 20;

    // Document undo history. A snapshot is pushed before every content
    // mutation (tree edits and text edits); `undo_stack` is newest last,
    // `redo_stack` newest first. `snapshot_undo`/`apply_undo`/`clear_undo`
    // are installed by the treeview. The undo dialog reads the stacks
    // directly and applies a rollback through `apply_undo`.
    std::vector<UndoState> undo_stack;
    std::vector<UndoState> redo_stack;
    static constexpr std::size_t kUndoMax = 50;

    std::function<void()> snapshot_undo;
    // `index` 0 = newest snapshot.
    std::function<void(std::size_t index)> apply_undo;
    std::function<void()> clear_undo;

    // File browser dialog. `show_file_browser` is the Modal visibility flag;
    // `browser_start_dir` is the directory to open in; `browser_pick` is
    // invoked with the chosen file's path when the user presses Enter.
    // `browser_command` is the `:command` name of the invoking op for
    // save/export-style ops: when non-empty, Enter on a folder closes the
    // dialog and reopens the command line prefilled with that folder so the
    // user can type a filename (open/import-style ops leave it empty and
    // Enter on a folder simply navigates into it).
    bool* show_file_browser = nullptr;
    std::string browser_start_dir;
    std::string browser_command;
    std::function<void(const std::string&)> browser_pick;

    std::function<void()> focus_editor;
    std::function<void()> focus_treeview;

    // Flat enumeration of the document: (node pointer, depth) in document
    // order, ignoring expansion state. Set by the treeview.
    std::function<std::vector<std::pair<terminadventure::treeview::TreeNode*, int>>()> collect_all_nodes;

    // Select `node` in the tree, expanding every ancestor so it is visible.
    // Set by the treeview.
    std::function<void(terminadventure::treeview::TreeNode*)> reveal_node;

    // Position the editor cursor on a 0-based line of the currently active
    // node. Set by the editor.
    std::function<void(int)> reveal_line;

    // Insert `text` into the currently active node at the editor cursor, as a
    // single undoable edit. Set by the editor; used by dialogs to inject text
    // (e.g. a node link picked from a search) at the cursor.
    std::function<void(const std::string&)> insert_text_at_cursor;

    // Vim-style find state (the last '/' search). `search_matches` holds the
    // nodes that matched the last query, in document order; `search_index` is
    // the currently selected match (n/N move it, wrapping). `search_active`
    // is cleared by ':noh' to hide the match highlight; the query and match
    // list are kept so n/N keep working, as in Vim.
    std::string search_query;
    std::vector<terminadventure::treeview::TreeNode*> search_matches;
    int search_index = -1;
    bool search_active = false;

    // Set when a '/' find executes; the editor consumes it to move its cursor
    // to the first occurrence in the (possibly unchanged) active node so the
    // match is revealed in the text, as in Vim.
    bool search_reveal_pending = false;

    // Step to the next (dir > 0) or previous (dir < 0) find result. Set by the
    // editor: it moves the cursor through the occurrences of the query inside
    // the active node's text (wrapping within the node), as in Vim.
    std::function<void(int)> search_jump;

    std::map<std::string, terminadventure::op::Operation> operations;
    std::map<std::string, std::string> commands;

    Keymap normal_keymap;
    Keymap insert_keymap;
    Keymap visual_keymap;
    Keymap visual_block_keymap;
    Keymap tree_keymap;

    Keymap& ActiveKeymap() {
        switch (mode) {
            case Mode::TREE:            return tree_keymap;
            case Mode::INSERT:          return insert_keymap;
            case Mode::VISUAL:
            case Mode::VISUAL_LINE:     return visual_keymap;
            case Mode::VISUAL_BLOCK:    return visual_block_keymap;
            case Mode::COMMAND:         return normal_keymap;
            default:                    return normal_keymap;
        }
    }
};
