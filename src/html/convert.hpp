#pragma once

#include <string>
#include <vector>

#include "../bookmark/bookmark.hpp"
#include "../treeview/tree_node.hpp"

namespace terminadventure::html
{
    using treeview::TreeNode;

    bool ImportHtmlFile(const std::string& path, std::vector<TreeNode>& roots,
                        std::vector<bookmark::Bookmark>* bookmarks = nullptr,
                        std::vector<std::string>* history = nullptr);
    bool ExportHtmlFile(const std::string& template_path, const std::string& out_path,
                        const std::vector<TreeNode>& roots,
                        const std::vector<bookmark::Bookmark>& bookmarks,
                        const std::vector<std::string>& history);
}
