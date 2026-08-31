#pragma once

#include <string>
#include <vector>

#include "node_type.hpp"

namespace terminadventure::treeview
{

    struct TreeNode
    {
        std::string id;
        std::string name;
        bool expanded = false;
        std::string text;
        NodeType type = NodeType::EDITOR;
        std::vector<TreeNode> children;
    };

}
