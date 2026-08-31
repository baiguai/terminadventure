// TreeView is the notes sidebar tree: an FTXUI component that owns the
// document's node tree, the current selection, and all tree-mode behavior:
//
//   - selection / cursor movement across the visible tree (j/k, gg/G, /find)
//   - structural edits: add, rename, delete and move nodes, expand & collapse
//   - document persistence: open, save, import and export
//   - undo history, scoped to the currently selected node
//   - the record of viewed nodes that powers the history dialog
//
// TreeView installs callbacks into EditorState.operations under formal op
// names ("move_up", "new_node", ...). The tree keymap in commands.conf maps
// keys to those names and op::HandleKey()/op::Dispatch() invoke the matching
// callback, so this file contains no key-decoding logic of its own.
#include "treeview.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <regex>
#include <set>
#include <string>
#include <utility>

#include "../bookmark/bookmark.hpp"
#include "../config/config.hpp"
#include "../history/history.hpp"
#include "../io/serialize.hpp"
#include "../html/convert.hpp"
#include "../op/op.hpp"
#include "../undo/undo.hpp"

namespace terminadventure::treeview
{

    int CountNodes(const std::vector<TreeNode>& nodes);
    TreeNode* FindParent(std::vector<TreeNode>& roots, TreeNode* child);
    TreeNode* FindById(std::vector<TreeNode>& nodes, const std::string& id);
    void CollectAllDepth(TreeNode& node, int depth, std::vector<std::pair<TreeNode*, int>>& out);
    void EnsureIds(std::vector<TreeNode>& nodes);
    std::vector<TreeNode> DefaultNodes();

    // ============ File scope helpers ============

    // Strip leading and trailing whitespace from s.
    std::string TrimWhitespace(const std::string& s)
    {
        std::size_t b = 0;
        std::size_t e = s.size();
        while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
        while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
        return s.substr(b, e - b);
    }

    // Strip the underscores of inter-note links (_text_ -> text) for the
    // plain-text note export, keeping surrounding whitespace/line breaks,
    // mirroring the regex used by the web app's exportNoteAsTxt().
    std::string StripNoteLinks(const std::string& content)
    {
        static const std::regex node_link_re(
            R"((^|\s)_([^_]|[^_].*?[^_])_(?=\s|[.,!?;:)]|$))");
        return std::regex_replace(content, node_link_re, "$1$2");
    }

    // ============ TreeView component ============

    // FTXUI component owning the document tree (roots_), the current
    // selection, and every tree-mode operation. Key events arrive through
    // the operation map installed in the constructor; this file never
    // decodes keys itself.
    class TreeView : public ftxui::ComponentBase
    {
        public:
            TreeView(std::shared_ptr<EditorState> state)
                : state_(std::move(state))
            {
                // Selection, expand/collapse and movement (j/k/h/l, gg/G, Enter).

                state_->operations["deselect_node"] = [this](const std::string&, int)
                {
                    if (state_->mode == Mode::TREE) selected_ = nullptr;
                    RefreshActiveNode();
                };
                state_->operations["move_up"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) MoveSelection(-count);
                    RefreshActiveNode();
                };
                state_->operations["move_down"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) MoveSelection(count);
                    RefreshActiveNode();
                };
                state_->operations["move_file_start"] = [this](const std::string&, int)
                {
                    if (state_->mode == Mode::TREE) MoveToStart();
                    RefreshActiveNode();
                };
                state_->operations["move_file_end"] = [this](const std::string&, int)
                {
                    if (state_->mode == Mode::TREE) MoveToEnd();
                    RefreshActiveNode();
                };
                state_->operations["tree_open"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) for (int i = 0; i < count; ++i) OpenSelected();
                    RefreshActiveNode();
                };
                state_->operations["tree_collapse"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) for (int i = 0; i < count; ++i) CollapseSelected();
                    RefreshActiveNode();
                };
                state_->operations["tree_expand"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE) for (int i = 0; i < count; ++i) ExpandSelected();
                    RefreshActiveNode();
                };
                state_->operations["expand_all"] = [this](const std::string&, int)
                {
                    if (state_->mode == Mode::TREE) ExpandAll();
                    RefreshActiveNode();
                };
                state_->operations["collapse_all"] = [this](const std::string&, int)
                {
                    if (state_->mode == Mode::TREE) CollapseAll();
                    RefreshActiveNode();
                };
                // Structural edits: add, rename, delete and reorder (a/A/R/D, J/K/H/L).

                state_->operations["new_node"] = [this](const std::string& name, int)
                {
                    if (!name.empty() && !(selected_ && IsProtected(FindParent(roots_, selected_))))
                    {
                        InsertNode(name);
                    }
                    RefreshActiveNode();
                };
                state_->operations["new_child"] = [this](const std::string& name, int)
                {
                    if (!name.empty() && !IsProtected(selected_))
                    {
                        InsertChild(name);
                    }
                    RefreshActiveNode();
                };
                state_->operations["rename_node"] = [this](const std::string& name, int)
                {
                    if (selected_ && !name.empty() && !IsProtected(selected_))
                    {
                        SnapshotUndo();
                        selected_->name = name;
                    }
                    RefreshActiveNode();
                };
                state_->operations["delete_node"] = [this](const std::string&, int)
                {
                    if (!IsProtected(selected_) && !IsProtected(FindParent(roots_, selected_)))
                    {
                        DeleteNode();
                    }
                    RefreshActiveNode();
                };
                state_->operations["move_node_up"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE && !IsProtected(selected_))
                        for (int i = 0; i < count; ++i) MoveNode(-1);
                    RefreshActiveNode();
                };
                state_->operations["move_node_down"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE && !IsProtected(selected_))
                        for (int i = 0; i < count; ++i) MoveNode(+1);
                    RefreshActiveNode();
                };
                state_->operations["move_parent_up"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE && !IsProtected(selected_))
                        for (int i = 0; i < count; ++i) MoveParent(-1);
                    RefreshActiveNode();
                };
                state_->operations["move_parent_down"] = [this](const std::string&, int count)
                {
                    if (state_->mode == Mode::TREE && !IsProtected(selected_))
                        for (int i = 0; i < count; ++i) MoveParent(+1);
                    RefreshActiveNode();
                };
                // Mode switches (i/I/:/Esc) and '/' search navigation (n/N).

                state_->operations["enter_normal"] = [this](const std::string&, int)
                {
                    if (selected_ == nullptr || IsProtected(selected_))
                    {
                        return;
                    }
                    state_->mode = Mode::NORMAL;
                    if (state_->focus_editor) state_->focus_editor();
                };
                state_->operations["enter_insert"] = [this](const std::string&, int)
                {
                    // ROLLER nodes show the dedicated dice-roller UI, and
                    // PLAYERS nodes the character-manager pane; both reuse
                    // INSERT mode for their interactive content.
                    const bool can_insert = selected_ != nullptr
                        && (!IsProtected(selected_)
                            || selected_->type == NodeType::ROLLER
                            || selected_->type == NodeType::PLAYERS);
                    if (!can_insert)
                    {
                        return;
                    }
                    state_->mode = Mode::INSERT;
                    if (state_->focus_editor) state_->focus_editor();
                };
                state_->operations["enter_tree"] = [this](const std::string&, int)
                {
                    state_->mode = Mode::TREE;
                    if (state_->focus_treeview) state_->focus_treeview();
                };
                state_->operations["enter_visual"] = [this](const std::string&, int)
                {
                    if (selected_ == nullptr || IsProtected(selected_))
                    {
                        return;
                    }
                    state_->mode = Mode::VISUAL;
                };
                state_->operations["enter_visual_line"] = [this](const std::string&, int)
                {
                    if (selected_ == nullptr || IsProtected(selected_))
                    {
                        return;
                    }
                    state_->mode = Mode::VISUAL_LINE;
                };
                state_->operations["enter_visual_block"] = [this](const std::string&, int)
                {
                    if (selected_ == nullptr || IsProtected(selected_))
                    {
                        return;
                    }
                    state_->mode = Mode::VISUAL_BLOCK;
                };
                state_->operations["enter_command"] = [this](const std::string&, int)
                {
                    terminadventure::op::OpenCommandLine(state_, "");
                };
                state_->operations["enter_search"] = [this](const std::string&, int)
                {
                    terminadventure::op::OpenSearchCommand(state_);
                };
                state_->operations["search_next"] = [this](const std::string&, int)
                {
                    if (state_->search_jump) state_->search_jump(+1);
                    else SearchJump(+1);
                };
                state_->operations["search_prev"] = [this](const std::string&, int)
                {
                    if (state_->search_jump) state_->search_jump(-1);
                    else SearchJump(-1);
                };
                state_->operations["search_clear"] = [this](const std::string&, int)
                {
                    state_->search_active = false;
                    state_->status = "Search highlight cleared";
                };
                // Document I/O: save, save-as, open, and HTML/text import/export.

                state_->operations["save"] = [this](const std::string&, int)
                {
                    if (current_file_.empty())
                    {
                        state_->status = "No file path set - use :saveas to save";
                        return;
                    }
                    SaveTo(current_file_);
                };
                state_->operations["saveas"] = [this](const std::string& path, int)
                {
                    if (path.empty())
                    {
                        state_->status = "Save as requires a path";
                        return;
                    }
                    if (IsDirectory(path))
                    {
                        BrowseFor(path, "saveas", [this](const std::string& chosen)
                                  {
                                      SaveTo(chosen);
                                  });
                        return;
                    }
                    SaveTo(path);
                };
                state_->operations["open"] = [this](const std::string& path, int)
                {
                    if (state_->changed)
                    {
                        state_->status =
                            "Unsaved changes - use :w to save or :o! to force";
                        return;
                    }
                    OpenFile(path);
                };
                state_->operations["open_force"] = [this](const std::string& path, int)
                {
                    OpenFile(path);
                };
                state_->operations["new_document"] = [this](const std::string&, int)
                {
                    if (state_->changed)
                    {
                        state_->status =
                            "Unsaved changes - use :w to save or :enew! to force";
                        return;
                    }
                    NewDocument();
                };
                state_->operations["new_document_force"] = [this](const std::string&, int)
                {
                    NewDocument();
                };
                state_->operations["import_html"] = [this](const std::string& path, int)
                {
                    if (path.empty())
                    {
                        state_->status = "Import requires a path";
                        return;
                    }
                    if (IsDirectory(path))
                    {
                        BrowseFor(path, "", [this](const std::string& chosen)
                                  {
                                      ImportFrom(chosen);
                                  });
                        return;
                    }
                    ImportFrom(path);
                };
                state_->operations["export_html"] = [this](const std::string& path, int)
                {
                    if (path.empty())
                    {
                        state_->status = "Export requires a path";
                        return;
                    }
                    if (IsDirectory(path))
                    {
                        BrowseFor(path, "X", [this](const std::string& chosen)
                                  {
                                      ExportTo(chosen);
                                  });
                        return;
                    }
                    ExportTo(path);
                };
                state_->operations["export_terminadventure"] = [this](const std::string& path, int)
                {
                    // With no node selected this is just a save-as of the whole
                    // document. Otherwise the selected node (and everything under
                    // it) is written out as its own Terminadventure file.
                    if (selected_ == nullptr)
                    {
                        if (path.empty() || IsDirectory(path))
                        {
                            BrowseFor(path.empty() ? ExportStartDir() : path,
                                      "saveas", [this](const std::string& chosen)
                                      {
                                          SaveTo(chosen);
                                      });
                            return;
                        }
                        SaveTo(path);
                        return;
                    }
                    if (path.empty() || IsDirectory(path))
                    {
                        BrowseFor(path.empty() ? ExportStartDir() : path,
                                  "x", [this](const std::string& chosen)
                                  {
                                      ExportTerminadventure(chosen);
                                  });
                        return;
                    }
                    ExportTerminadventure(path);
                };
                state_->operations["export_note_txt"] = [this](const std::string& path, int)
                {
                    if (path.empty() || IsDirectory(path))
                    {
                        BrowseFor(path.empty() ? ExportStartDir() : path,
                                  "export_note_txt", [this](const std::string& chosen)
                                  {
                                      ExportNoteTxt(chosen);
                                  });
                        return;
                    }
                    ExportNoteTxt(path);
                };
                state_->operations["export_tree_txt"] = [this](const std::string& path, int)
                {
                    if (path.empty() || IsDirectory(path))
                    {
                        BrowseFor(path.empty() ? ExportStartDir() : path,
                                  "export_tree_txt", [this](const std::string& chosen)
                                  {
                                      ExportTreeTxt(chosen);
                                  });
                        return;
                    }
                    ExportTreeTxt(path);
                };
                // Tree pane width (] widen, [ narrow).

                state_->operations["treeview_width_increase"] = [this](const std::string&, int count)
                {
                    state_->treeview_width = std::min(kMaxTreeviewWidth,
                        state_->treeview_width + std::max(1, count));
                };
                state_->operations["treeview_width_decrease"] = [this](const std::string&, int count)
                {
                    state_->treeview_width = std::max(kMinTreeviewWidth,
                        state_->treeview_width - std::max(1, count));
                };

                // Callbacks the rest of the app uses to inspect or drive the tree.

                state_->collect_all_nodes = [this]
                {
                    std::vector<std::pair<TreeNode*, int>> out;
                    for (auto& root : roots_)
                    {
                        CollectAllDepth(root, 0, out);
                    }
                    return out;
                };
                state_->reveal_node = [this](TreeNode* target)
                {
                    if (!target) return;
                    for (TreeNode* cur = target; cur; cur = FindParent(roots_, cur))
                    {
                        if (cur != target) cur->expanded = true;
                    }
                    selected_ = target;
                    RefreshActiveNode();
                };

                state_->snapshot_undo = [this] { SnapshotUndo(); };
                state_->apply_undo = [this](std::size_t index) { ApplyUndo(index); };
                state_->clear_undo = [this] { ClearUndo(); };

                if (roots_.empty())
                {
                    roots_ = DefaultNodes();
                }

                RefreshActiveNode();
            }
            bool Focusable() const override
            {
                return true;
            }
            bool OnEvent(ftxui::Event event) override;
            ftxui::Element Render() override;

        private:
            std::vector<TreeNode*> VisibleNodes();
            std::vector<TreeNode>& ContainerOf(TreeNode* node);
            void MoveSelection(int dir);
            void MoveToStart();
            void MoveToEnd();
            void ExpandSelected();
            void CollapseSelected();
            void OpenSelected();
            void ExpandAll();
            void CollapseAll();
            void InsertChild(const std::string& name);
            void InsertNode(const std::string& name);
            void DeleteNode();
            void MoveNode(int dir);
            void MoveParent(int dir);
            bool IsProtected(const TreeNode* node) const;
            void SearchJump(int dir);
            void RefreshActiveNode();
            void SaveTo(const std::string& path);
            void LoadFrom(const std::string& path);
            void OpenFile(const std::string& path);
            void NewDocument();
            void ImportFrom(const std::string& path);
            void ExportTo(const std::string& path);
            void ExportNoteTxt(const std::string& path);
            void ExportTreeTxt(const std::string& path);
            void ExportTerminadventure(const std::string& path);
            std::string CollectBranchTxt(const TreeNode& node) const;
            std::string ExportStartDir() const;
            void PersistLastFile();
            void PushRecentFile(const std::string& path);
            void SnapshotUndo();
            void ApplyUndo(std::size_t index);
            void ClearUndo();
            bool IsDirectory(const std::string& path);
            void BrowseFor(const std::string& dir, const std::string& command,
                           std::function<void(const std::string&)> on_pick);
            bool IsAncestor(TreeNode& ancestor, TreeNode* node);
            void CollectVisibleDepth(TreeNode& node, int depth, std::vector<TreeNode*>& nodes, std::vector<int>& depths);

            std::shared_ptr<EditorState> state_;

            // The document's root nodes; every other node hangs below these.
            std::vector<TreeNode> roots_;

            // The currently selected node (null = deselect). selected_id_
            // mirrors its id so RefreshActiveNode() detects when a selection
            // change must reset the undo history.
            TreeNode* selected_ = nullptr;
            std::string selected_id_;

            // Path of the file backing this tree ("" when unsaved/imported).
            std::string current_file_;
    };

    // ============ TreeView factory ============

    // Component factory: give EditorState a ready-to-render TreeView.
    ftxui::Component MakeTreeView(std::shared_ptr<EditorState> state)
    {
        return ftxui::Make<TreeView>(std::move(state));
    }

    // ============ Tree traversal helpers ============

    // Append `node` and, when it is expanded, all of its visible descendants.
    void CollectVisible(TreeNode& node, std::vector<TreeNode*>& out)
    {
        out.push_back(&node);
        if (!node.children.empty() && node.expanded)
        {
            for (auto& child : node.children)
            {
                CollectVisible(child, out);
            }
        }
    }

    // Depth-first walk that ignores expansion state; every node is appended with
    // its depth (this is the "whole document" enumeration).
    void CollectAllDepth(TreeNode& node, int depth, std::vector<std::pair<TreeNode*, int>>& out)
    {
        out.push_back({&node, depth});
        for (auto& child : node.children)
        {
            CollectAllDepth(child, depth + 1, out);
        }
    }

    // Apply `expanded` to every node in the subtree that has children.
    void SetAllExpanded(TreeNode& node, bool expanded, bool skip_root)
    {
        if (!node.children.empty() && !skip_root)
        {
            node.expanded = expanded;
        }
        for (auto& child : node.children)
        {
            SetAllExpanded(child, expanded, false);
        }
    }

    // Find the node that directly owns `child`, or null when it is a root.
    TreeNode* FindParent(TreeNode& node, TreeNode* child)
    {
        for (auto& c : node.children)
        {
            if (&c == child)
            {
                return &node;
            }
            if (TreeNode* parent = FindParent(c, child))
            {
                return parent;
            }
        }
        return nullptr;
    }

    // FindParent() over a whole root list.
    TreeNode* FindParent(std::vector<TreeNode>& roots, TreeNode* child)
    {
        for (auto& root : roots)
        {
            if (TreeNode* parent = FindParent(root, child))
            {
                return parent;
            }
        }
        return nullptr;
    }

    // Depth-first search for the node with `id`, or null.
    TreeNode* FindById(std::vector<TreeNode>& nodes, const std::string& id)
    {
        for (auto& node : nodes)
        {
            if (node.id == id) return &node;
            if (TreeNode* found = FindById(node.children, id)) return found;
        }
        return nullptr;
    }

    // Total number of nodes in the forest (includes all children).
    int CountNodes(const std::vector<TreeNode>& nodes)
    {
        int total = 0;
        for (const auto& node : nodes)
        {
            ++total;
            total += CountNodes(node.children);
        }
        return total;
    }

    // The sibling list that owns `node`: its parent's children, or the roots.
    std::vector<TreeNode>& TreeView::ContainerOf(TreeNode* node)
    {
        TreeNode* parent = FindParent(roots_, node);
        return parent ? parent->children : roots_;
    }

    // All nodes currently on screen in display order: a depth-first walk of
    // only the expanded nodes.
    std::vector<TreeNode*> TreeView::VisibleNodes()
    {
        std::vector<TreeNode*> out;
        for (auto& root : roots_)
        {
            CollectVisible(root, out);
        }
        return out;
    }

    // True when `ancestor` is an ancestor of `node` (or is node itself).
    bool TreeView::IsAncestor(TreeNode& ancestor, TreeNode* node)
    {
        for (TreeNode* cur = node; cur; cur = FindParent(roots_, cur))
        {
            if (cur == &ancestor)
            {
                return true;
            }
        }
        return false;
    }

    // Same walk as VisibleNodes(), but also reporting each node's depth.
    void TreeView::CollectVisibleDepth(TreeNode& node, int depth,
                                       std::vector<TreeNode*>& nodes,
                                       std::vector<int>& depths)
    {
        nodes.push_back(&node);
        depths.push_back(depth);
        if (!node.children.empty() && node.expanded)
        {
            for (auto& child : node.children)
            {
                CollectVisibleDepth(child, depth + 1, nodes, depths);
            }
        }
    }

    // ============ Selection movement and search ============

    // Step the selection `dir` rows (-1 up, +1 down) through the visible
    // nodes, clamped to the ends; selects the first node when none is set.
    void TreeView::MoveSelection(int dir)
    {
        auto visible = VisibleNodes();
        if (visible.empty())
        {
            return;
        }
        for (std::size_t i = 0; i < visible.size(); ++i)
        {
            if (visible[i] == selected_)
            {
                int next = static_cast<int>(i) + dir;
                next = std::max(0, std::min(static_cast<int>(visible.size()) - 1, next));
                selected_ = visible[static_cast<std::size_t>(next)];
                return;
            }
        }
        selected_ = visible.front();
    }

    // Move to the next/previous node in the '/' search result list (n/N),
    // wrapping at both ends, and reveal the picked node.
    void TreeView::SearchJump(int dir)
    {
        const auto& matches = state_->search_matches;
        if (matches.empty())
        {
            state_->status = "No search";
            return;
        }
        const int n = static_cast<int>(matches.size());
        int idx = state_->search_index;
        if (idx < 0) idx = (dir > 0) ? 0 : n - 1;
        else idx = (idx + dir + n) % n;
        state_->search_index = idx;
        if (state_->reveal_node) state_->reveal_node(matches[static_cast<std::size_t>(idx)]);
        state_->status = "Match " + std::to_string(idx + 1) + " of " + std::to_string(n);
    }

    // ============ Node reordering ============

    // Swap the selected node with its sibling at offset `dir` (-1 up, +1
    // down), reordering it within the same parent.
    void TreeView::MoveNode(int dir)
    {
        if (selected_ == nullptr) return;
        SnapshotUndo();
        auto& children = ContainerOf(selected_);
        for (std::size_t i = 0; i < children.size(); ++i)
        {
            if (&children[i] != selected_) continue;
            std::size_t j = static_cast<std::size_t>(static_cast<int>(i) + dir);
            if (j >= children.size()) return;
            std::swap(children[i], children[j]);
            selected_ = &children[j];
            return;
        }
    }

    // Change the selected node's level: dir < 0 promotes it one level up (out
    // of its parent), dir > 0 demotes it one level down (under its previous
    // sibling).
    void TreeView::MoveParent(int dir)
    {
        if (selected_ == nullptr) return;
        SnapshotUndo();
        auto& children = ContainerOf(selected_);
        std::size_t si = 0;
        while (si < children.size() && &children[si] != selected_) ++si;
        if (si >= children.size()) return;

        if (dir < 0)
        {
            TreeNode* parent = FindParent(roots_, selected_);
            if (!parent) return;
            auto& container = ContainerOf(parent);
            std::size_t pi = 0;
            while (pi < container.size() && &container[pi] != parent) ++pi;
            if (pi >= container.size()) return;

            TreeNode moved = std::move(children[si]);
            children.erase(children.begin() + static_cast<std::ptrdiff_t>(si));

            auto it = container.insert(container.begin() + static_cast<std::ptrdiff_t>(pi) + 1, std::move(moved));
            selected_ = &*it;
        }
        else
        {
            std::vector<TreeNode*> visible;
            std::vector<int> depth;
            for (auto& root : roots_)
            {
                CollectVisibleDepth(root, 0, visible, depth);
            }

            std::size_t v = 0;
            while (v < visible.size() && visible[v] != selected_) ++v;
            if (v >= visible.size()) return;

            // Nest under the node directly above at the same level (the
            // previous sibling): moving a node down a level makes it the
            // child of the node above it, not of that node's deepest
            // descendant. (The old folder/note model found the deepest
            // folder above; with everything a note that skips past the
            // siblings and drops the node several levels too deep.)
            TreeNode* target = nullptr;
            for (std::size_t i = v; i-- > 0;)
            {
                if (depth[i] == depth[v])
                {
                    target = visible[i];
                    break;
                }
            }
            if (!target) return;

            TreeNode moved = std::move(children[si]);
            children.erase(children.begin() + static_cast<std::ptrdiff_t>(si));

            target->expanded = true;
            target->children.insert(target->children.begin(), std::move(moved));
            selected_ = &target->children.front();
        }
    }

    // A node the user must not modify through the tree/editor: anything that is
    // not a plain EDITOR node (it renders through a dedicated UI), and the
    // DM Tools root regardless of type.
    bool TreeView::IsProtected(const TreeNode* node) const
    {
        if (node == nullptr) return false;
        if (node->type != NodeType::EDITOR) return true;
        return node->name == "DM Tools";
    }

    // Sync state_->active_node with the selection, reset the undo history when
    // the selection changes, and record the viewed node into the history list.
    void TreeView::RefreshActiveNode()
    {
        state_->active_node = selected_;
        const std::string cur_id = selected_ ? selected_->id : "";
        // Undo history is scoped to the currently selected node: switching to
        // a different node (or deselecting) resets it, leaving one baseline
        // snapshot of the node's text as it is at selection time.
        if (cur_id != selected_id_)
        {
            selected_id_ = cur_id;
            ClearUndo();
            if (selected_ != nullptr)
            {
                SnapshotUndo();
            }
        }
        if (selected_ != nullptr && !selected_->text.empty())
        {
            terminadventure::history::Record(*state_, selected_->id);
        }
        // A ROLLER node renders through the dedicated dice-roller UI and
        // reuses INSERT mode: focus the right pane so typing works. The same
        // applies to PLAYERS nodes, which show the character-manager pane.
        if (selected_ != nullptr && state_->mode == Mode::TREE
            && (selected_->type == NodeType::ROLLER || selected_->type == NodeType::PLAYERS))
        {
            state_->mode = Mode::INSERT;
            if (state_->focus_editor) state_->focus_editor();
        }
    }

    // ============ Document persistence ============

    // Serialize the whole document and write it to `path`, remembering the
    // path for future saves and updating the recent-files list and init file.
    void TreeView::SaveTo(const std::string& path)
    {
        std::string json = terminadventure::io::Serialize(roots_, state_->treeview_width,
                                                      state_->bookmarks,
                                                      state_->history, state_->presets,
                                                      state_->players);
        if (!terminadventure::io::SaveDocumentFile(path, json))
        {
            state_->status = "Error: could not write " + path;
            return;
        }
        current_file_ = path;
        state_->changed = false;
        state_->status = "Saved " + std::to_string(CountNodes(roots_)) + " nodes to " + path;
        PushRecentFile(path);
        PersistLastFile();
    }

    // Read and deserialize a document (tree, pane width, bookmarks and history)
    // from `path`, making it the currently edited file.
    void TreeView::LoadFrom(const std::string& path)
    {
        std::string content;
        const terminadventure::io::LoadStatus status =
            terminadventure::io::LoadDocumentFile(path, content);
        if (status == terminadventure::io::LoadStatus::NotFound)
        {
            state_->status = "Error: could not open " + path;
            return;
        }
        if (status == terminadventure::io::LoadStatus::NotTerminadventure)
        {
            state_->status = "Not a Terminadventure file: " + path;
            return;
        }
        std::vector<TreeNode> loaded;
        int loaded_width = state_->treeview_width;
        std::vector<terminadventure::bookmark::Bookmark> loaded_marks;
        std::vector<std::string> loaded_history;
        std::vector<std::string> loaded_presets;
        std::vector<terminadventure::players::Player> loaded_players;
        if (!terminadventure::io::Deserialize(content, loaded, &loaded_width, &loaded_marks,
                                          &loaded_history, &loaded_presets, &loaded_players))
        {
            state_->status = "Error: could not parse " + path;
            return;
        }
        state_->treeview_width = loaded_width;
        state_->bookmarks = std::move(loaded_marks);
        state_->history = std::move(loaded_history);
        state_->presets = std::move(loaded_presets);
        state_->players = std::move(loaded_players);
        EnsureIds(loaded);
        roots_ = std::move(loaded);
        current_file_ = path;
        selected_ = nullptr;
        ClearUndo();
        RefreshActiveNode();
        state_->changed = false;
        state_->status = "Loaded " + std::to_string(CountNodes(roots_)) + " nodes from " + path;
        PushRecentFile(path);
        PersistLastFile();
    }

    // Load a document from the HTML export produced by the web app; unlike
    // LoadFrom() this leaves the document without a file path.
    void TreeView::ImportFrom(const std::string& path)
    {
        std::vector<TreeNode> loaded;
        std::vector<terminadventure::bookmark::Bookmark> loaded_marks;
        std::vector<std::string> loaded_history;
        if (!terminadventure::html::ImportHtmlFile(path, loaded, &loaded_marks, &loaded_history))
        {
            state_->status = "Error: could not import " + path;
            return;
        }
        EnsureIds(loaded);
        state_->bookmarks = std::move(loaded_marks);
        state_->history = std::move(loaded_history);
        roots_ = std::move(loaded);
        current_file_.clear();
        selected_ = nullptr;
        ClearUndo();
        RefreshActiveNode();
        state_->changed = false;
        state_->status = "Imported " + std::to_string(CountNodes(roots_)) + " nodes from " + path;
        // PushRecentFile(path);
        PersistLastFile();
    }

    // Open `path` directly, or launch the file browser when it is a folder.
    void TreeView::OpenFile(const std::string& path)
    {
        if (path.empty())
        {
            state_->status = "Open requires a path";
            return;
        }
        if (IsDirectory(path))
        {
            BrowseFor(path, "", [this](const std::string& chosen)
                      {
                          LoadFrom(chosen);
                      });
            return;
        }
        LoadFrom(path);
    }

    // Build the default document skeleton: a DM Tools root holding the
    // standard per-campaign folders, plus a plain Notes root.
    std::vector<TreeNode> DefaultNodes()
    {
        std::vector<TreeNode> folders;
        folders.reserve(4);
        TreeNode roller;
        roller.id = terminadventure::bookmark::NewId();
        roller.name = "Dice Roller";
        roller.type = NodeType::ROLLER;
        TreeNode players;
        players.id = terminadventure::bookmark::NewId();
        players.name = "Players";
        players.type = NodeType::PLAYERS;
        TreeNode enemies;
        enemies.id = terminadventure::bookmark::NewId();
        enemies.name = "Enemies";
        enemies.type = NodeType::ENEMIES;
        folders.push_back(std::move(roller));
        folders.push_back(std::move(players));
        folders.push_back(std::move(enemies));

        TreeNode dm;
        dm.id = terminadventure::bookmark::NewId();
        dm.name = "DM Tools";
        dm.expanded = true;
        dm.children = std::move(folders);

        TreeNode notes;
        notes.id = terminadventure::bookmark::NewId();
        notes.name = "Notes";

        std::vector<TreeNode> roots;
        roots.push_back(std::move(dm));
        roots.push_back(std::move(notes));
        return roots;
    }

    // Clear the tree, selection, bookmarks, history and undo; reset the pane
    // width and leave the document without a file path. A fresh document
    // starts from the default node skeleton.
    void TreeView::NewDocument()
    {
        roots_ = DefaultNodes();
        current_file_.clear();
        selected_ = nullptr;
        state_->treeview_width = kDefaultTreeviewWidth;
        state_->bookmarks.clear();
        state_->history.clear();
        ClearUndo();
        PersistLastFile();
        RefreshActiveNode();
        state_->changed = false;
        state_->status = "New document - no file path";
    }

    // Write the web-app HTML export: patch an existing target file in place so
    // manual edits survive re-export, otherwise build from the terminadventure.html
    // template.
    void TreeView::ExportTo(const std::string& path)
    {
        // When overwriting an existing export, patch the data in that file in
        // place rather than regenerating from the config template, so any other
        // edits made to the exported file are preserved. Only the tree,
        // bookmarks and history data sections change.
        std::error_code ec;
        const bool overwrite = std::filesystem::exists(path, ec);
        const std::string base = overwrite ? path : state_->template_path;

        if (base.empty())
        {
            state_->status = "Error: terminadventure.html template not found";
            return;
        }
        if (!terminadventure::html::ExportHtmlFile(base, path,
                                               roots_, state_->bookmarks,
                                               state_->history))
        {
            state_->status = "Error: could not export " + path;
            return;
        }
        state_->status = "Exported " + std::to_string(CountNodes(roots_)) + " nodes to " + path;
    }

    // Export the selected node and its whole subtree as a standalone
    // Terminadventure document (its own .terminadventure file). Unlike SaveTo this
    // does not repoint current_file_, touch the recent-files list or rewrite
    // init.conf: the branch is written to `path` and the open document is
    // left exactly as it was. Bookmarks and history are not carried over and
    // the pane width is preserved so the file reopens with the same layout.
    void TreeView::ExportTerminadventure(const std::string& path)
    {
        if (selected_ == nullptr)
        {
            state_->status = "No node selected";
            return;
        }
        std::vector<TreeNode> branch;
        branch.push_back(*selected_);
        const std::string json = terminadventure::io::Serialize(branch, state_->treeview_width,
                                                             {}, {}, {}, {});
        if (!terminadventure::io::SaveDocumentFile(path, json))
        {
            state_->status = "Error: could not export " + path;
            return;
        }
        state_->status = "Exported " + std::to_string(CountNodes(branch)) + " nodes to " + path;
    }

    // Export the selected note's text as plain text, with inter-note link
    // underscores stripped (web app's exportNoteAsTxt()).
    void TreeView::ExportNoteTxt(const std::string& path)
    {
        if (selected_ == nullptr)
        {
            state_->status = "No note selected";
            return;
        }
        if (selected_->text.empty())
        {
            state_->status = "Note is empty";
            return;
        }
        const std::string content = StripNoteLinks(selected_->text);
        if (!terminadventure::io::WriteFile(path, content))
        {
            state_->status = "Error: could not export " + path;
            return;
        }
        state_->status = "Exported note to " + path;
    }

    // Export the selected branch as plain text, depth-first: each node's text
    // is written verbatim followed by four blank lines; nodes whose text
    // contains "#noexp" are skipped (web app's exportTreeAsTxt()).
    void TreeView::ExportTreeTxt(const std::string& path)
    {
        if (selected_ == nullptr)
        {
            state_->status = "No node selected";
            return;
        }
        const std::string content = CollectBranchTxt(*selected_);
        if (!terminadventure::io::WriteFile(path, content))
        {
            state_->status = "Error: could not export " + path;
            return;
        }
        state_->status = "Exported branch to " + path;
    }

    // Depth-first plain-text dump of a branch: each node's text followed by
    // four blank lines, skipping empty and "#noexp" nodes.
    std::string TreeView::CollectBranchTxt(const TreeNode& node) const
    {
        std::string out;
        if (!TrimWhitespace(node.text).empty()
            && node.text.find("#noexp") == std::string::npos)
        {
            out += node.text;
            out += "\n\n\n\n";
        }
        for (const auto& child : node.children)
        {
            out += CollectBranchTxt(child);
        }
        return out;
    }

    // Hitting Enter w/out entering a path shows the dialog, defaulted to the
    // folder of the document currently being edited, or the working directory
    // when no file is loaded yet.
    std::string TreeView::ExportStartDir() const
    {
        if (!current_file_.empty())
        {
            const std::string parent =
                std::filesystem::path(current_file_).parent_path().string();
            if (!parent.empty()) return parent;
        }
        std::error_code ec;
        return std::filesystem::current_path(ec).string();
    }

    // True when `path` names an existing directory (errors count as false).
    bool TreeView::IsDirectory(const std::string& path)
    {
        std::error_code ec;
        return std::filesystem::is_directory(path, ec);
    }

    // Store the last-edited file path (and recent list) into init.conf so the
    // tree is reopened on the next launch.
    void TreeView::PersistLastFile()
    {
        if (state_->init_path.empty()) return;
        terminadventure::config::WriteInit(state_->init_path, current_file_, state_->recent_files);
    }

    // Move `path` to the front of the recent-files list, capping its size.
    void TreeView::PushRecentFile(const std::string& path)
    {
        if (path.empty()) return;
        auto& recent = state_->recent_files;
        recent.erase(std::remove(recent.begin(), recent.end(), path), recent.end());
        recent.insert(recent.begin(), path);
        if (recent.size() > EditorState::kRecentMax)
        {
            recent.resize(EditorState::kRecentMax);
        }
    }

    // ============ Undo history ============

    // Push a full serialized snapshot of the document onto the undo stack
    // (skipped when identical to the top, capped in size), clearing redo.
    void TreeView::SnapshotUndo()
    {
        std::string json = terminadventure::io::Serialize(roots_, state_->treeview_width,
                                                      state_->bookmarks,
                                                      state_->history, state_->presets,
                                                      state_->players);
        auto& stack = state_->undo_stack;
        if (!stack.empty() && stack.back().json == json) return;
        UndoState st;
        st.json = std::move(json);
        st.preview = selected_ ? terminadventure::undo::FirstTextLine(*selected_) : "";
        stack.push_back(std::move(st));
        state_->redo_stack.clear();
        if (stack.size() > EditorState::kUndoMax)
        {
            stack.erase(stack.begin());
        }
    }

    // Roll back to undo entry `index` (0 = newest): move the current state to
    // the redo stack, drop newer undos, reload the stored tree, and re-find
    // the previously selected node by id.
    void TreeView::ApplyUndo(std::size_t index)
    {
        auto& stack = state_->undo_stack;
        if (index >= stack.size()) return;
        const std::size_t stack_index = stack.size() - 1 - index;

        UndoState current;
        current.json = terminadventure::io::Serialize(roots_, state_->treeview_width,
                                                  state_->bookmarks, state_->history,
                                                  state_->presets, state_->players);
        current.preview = selected_ ? terminadventure::undo::FirstTextLine(*selected_) : "";
        state_->redo_stack.push_back(std::move(current));
        for (std::size_t i = stack.size(); i-- > stack_index + 1;)
        {
            state_->redo_stack.push_back(stack[i]);
        }
        if (state_->redo_stack.size() > EditorState::kUndoMax)
        {
            state_->redo_stack.erase(
                state_->redo_stack.begin(),
                state_->redo_stack.begin() +
                    static_cast<std::ptrdiff_t>(state_->redo_stack.size() - EditorState::kUndoMax));
        }

        const std::string selected_id = selected_ ? selected_->id : "";

        const UndoState& target = stack[stack_index];
        std::vector<TreeNode> loaded;
        int width = state_->treeview_width;
        std::vector<bookmark::Bookmark> marks;
        std::vector<std::string> hist;
        std::vector<std::string> pres;
        std::vector<terminadventure::players::Player> plrs;
        if (!terminadventure::io::Deserialize(target.json, loaded, &width, &marks, &hist, &pres,
                                          &plrs))
        {
            state_->status = "Undo failed: stored state unreadable";
            return;
        }
        roots_ = std::move(loaded);
        state_->treeview_width = width;
        state_->bookmarks = std::move(marks);
        state_->history = std::move(hist);
        state_->presets = std::move(pres);
        state_->players = std::move(plrs);
        state_->changed = true;
        selected_ = nullptr;
        if (!selected_id.empty())
        {
            selected_ = FindById(roots_, selected_id);
        }
        selected_id_ = selected_ ? selected_->id : "";
        if (selected_ != nullptr && state_->reveal_node)
        {
            state_->reveal_node(selected_);
        }
        else
        {
            RefreshActiveNode();
        }
        stack.resize(stack_index);
    }

    // Drop both the undo and redo stacks.
    void TreeView::ClearUndo()
    {
        state_->undo_stack.clear();
        state_->redo_stack.clear();
    }

    // Open the file-browser dialog. `on_pick` runs when a file is chosen; when
    // `command` is non-empty, choosing a folder instead reopens the command
    // line prefilled with it (the save/export flow).
    void TreeView::BrowseFor(const std::string& dir, const std::string& command,
                             std::function<void(const std::string&)> on_pick)
    {
        if (!state_->show_file_browser) return;
        state_->browser_start_dir = dir;
        state_->browser_command = command;
        state_->browser_pick = std::move(on_pick);
        *state_->show_file_browser = true;
    }

    // ============ Selection movement and expand/collapse ============

    // Select the first visible node (gg).
    void TreeView::MoveToStart()
    {
        auto visible = VisibleNodes();
        if (!visible.empty())
        {
            selected_ = visible.front();
        }
    }

    // Select the last visible node (G).
    void TreeView::MoveToEnd()
    {
        auto visible = VisibleNodes();
        if (!visible.empty())
        {
            selected_ = visible.back();
        }
    }

    // Drill "in": expand the selected folder, or descend into its first child
    // when it is already expanded.
    void TreeView::ExpandSelected()
    {
        if (selected_ == nullptr || selected_->children.empty()) return;
        if (!selected_->expanded) { selected_->expanded = true; return; }
        selected_ = &selected_->children.front();
    }

    // Drill "out": collapse the selected folder, or move up to its parent when
    // it is already collapsed.
    void TreeView::CollapseSelected()
    {
        if (selected_ == nullptr) return;
        if (!selected_->children.empty() && selected_->expanded)
        {
            selected_->expanded = false;
            return;
        }
        if (TreeNode* parent = FindParent(roots_, selected_))
        {
            selected_ = parent;
        }
    }

    // Toggle the selected folder's expansion (Enter on a folder).
    void TreeView::OpenSelected()
    {
        if (selected_ == nullptr || selected_->children.empty()) return;
        selected_->expanded = !selected_->expanded;
    }

    // Expand every folder in the tree (E).
    void TreeView::ExpandAll()
    {
        for (auto& root : roots_)
        {
            SetAllExpanded(root, true, false);
        }
    }

    // Collapse every folder in the tree (C); deselect when the selection ends
    // up hidden.
    void TreeView::CollapseAll()
    {
        for (auto& root : roots_)
        {
            SetAllExpanded(root, false, false);
        }
        auto visible = VisibleNodes();
        if (std::find(visible.begin(), visible.end(), selected_) == visible.end())
        {
            selected_ = nullptr;
        }
    }

    // ============ Node creation and deletion ============

    // Build a fresh node with a new id and the given name.
    TreeNode new_node(std::string name)
    {
        TreeNode node;
        node.id = terminadventure::bookmark::NewId();
        node.name = std::move(name);
        return node;
    }

    // Give every node that lacks one a fresh id (after import or load).
    void EnsureIds(std::vector<TreeNode>& nodes)
    {
        for (auto& node : nodes)
        {
            if (node.id.empty()) node.id = terminadventure::bookmark::NewId();
            EnsureIds(node.children);
        }
    }

    // Add `name` as the first child of the selected node (A), expanding it; with
    // no selection the new node becomes a root.
    void TreeView::InsertChild(const std::string& name)
    {
        SnapshotUndo();
        if (selected_ == nullptr)
        {
            roots_.push_back(new_node(name));
            selected_ = &roots_.back();
            return;
        }
        selected_->expanded = true;
        auto inserted = selected_->children.insert(selected_->children.begin(), new_node(name));
        selected_ = &*inserted;
    }

    // Add `name` as the sibling just below the selected node (a); with no
    // selection the new node becomes a root.
    void TreeView::InsertNode(const std::string& name)
    {
        SnapshotUndo();
        if (selected_ == nullptr)
        {
            roots_.push_back(new_node(name));
            selected_ = &roots_.back();
            return;
        }
        auto& children = ContainerOf(selected_);
        for (auto it = children.begin(); it != children.end(); ++it)
        {
            if (&*it == selected_)
            {
                auto inserted = children.insert(it + 1, new_node(name));
                selected_ = &*inserted;
                return;
            }
        }
    }

    // Remove the selected node (D), then select its next sibling, the parent, or
    // nothing, whichever is available.
    void TreeView::DeleteNode()
    {
        if (selected_ == nullptr) return;
        SnapshotUndo();
        TreeNode* parent = FindParent(roots_, selected_);
        auto& children = ContainerOf(selected_);
        for (std::size_t i = 0; i < children.size(); ++i)
        {
            if (&children[i] != selected_) continue;

            children.erase(children.begin() + static_cast<std::ptrdiff_t>(i));

            if (!children.empty())
            {
                std::size_t next = std::min(i, children.size() -1);
                selected_ = &children[next];
            }
            else
            {
                selected_ = parent;
            }
            RefreshActiveNode();
            return;
        }
    }

    // Forward every key event to the shared operation dispatcher; the tree
    // keymap is what actually decides what a key means.
    bool TreeView::OnEvent(ftxui::Event event)
    {
        return terminadventure::op::HandleKey(state_, event);
    }

    // ============ Rendering ============

    // Render one node as a row (indented by depth, ▸/▾ folder marker) and
    // recurse into its expanded children. Yellow = search match, inverted =
    // this node is selected.
    void RenderNode(const TreeNode& node, int depth, const TreeNode* selected,
                    const std::set<const TreeNode*>* matches, ftxui::Elements& rows)
    {
        std::string indent(depth * 2, ' ');
        std::string marker = !node.children.empty() ? (node.expanded ? "▾" : "▸") : " ";
        auto row = ftxui::text(indent + marker + " " + node.name);
        if (matches != nullptr && matches->find(&node) != matches->end())
        {
            row |= ftxui::bgcolor(ftxui::Color::Yellow);
        }
        if (&node == selected)
        {
            row |= ftxui::inverted;
            row |= ftxui::focus;
        }
        rows.push_back(row);

        if (node.children.empty() || !node.expanded)
        {
            return;
        }
        for (const auto& child : node.children)
        {
            RenderNode(child, depth + 1, selected, matches, rows);
        }
    }

    // Compose the full tree pane: all root rows in a scrollable, flexible
    // vertical box.
    ftxui::Element TreeView::Render()
    {
        std::set<const TreeNode*> matches;
        if (state_->search_active)
        {
            for (const TreeNode* m : state_->search_matches)
            {
                matches.insert(m);
            }
        }
        ftxui::Elements rows;
        for (const auto& root : roots_)
        {
            RenderNode(root, 0, selected_, &matches, rows);
        }
        return ftxui::vbox(std::move(rows)) | ftxui::frame | ftxui::flex;
    }

}
