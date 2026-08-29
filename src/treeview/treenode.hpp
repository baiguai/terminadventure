#pragma once

#include <string>
#include <vector>

namespace terminadventure::treeview
{
    struct TreeNode
    {
        std::string id;
        std::string name;
        bool expanded { false };
        std::string text;
        std::string type;
        std::vector<TreeNode> children;
    };
}
