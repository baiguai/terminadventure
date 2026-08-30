#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace terminadventure::treeview
{
    struct TreeNode;
}

namespace terminadventure::links
{
    // A link found in a note's text: `label` is the display text (markdown
    // label, or the target itself), `target` is the raw reference (a URL or a
    // `_Title_`), `node` is the resolved target node once `Recompute` fills it
    // in, and `ok` says whether the target resolves. `from_markdown` marks
    // `[label](target)` style links.
    struct Link
    {
        std::string label;
        std::string target;
        treeview::TreeNode* node = nullptr;
        bool from_markdown = false;
        bool ok = true;
    };

    std::string Trim(const std::string& s);
    std::string Lower(const std::string& s);
    bool IsUrlTarget(const std::string& target);
    bool IsNoteTarget(const std::string& target);
    std::string NoteTitle(const std::string& target);

    // Collect every link in `content`, deduplicated by target, in order.
    // Mirrors the HTML app's openLinksDialog() scanning rules.
    void CollectLinks(const std::string& content, std::vector<Link>& out);

    ftxui::Component MakeLinksDialog(std::shared_ptr<EditorState> state, bool* show);
}
