#pragma once

#include <string>
#include <vector>

namespace terminadventure::bookmark
{
    // A bookmark that points at a tree node.
    //   line == -1: node bookmark (targets the node itself)
    //   line >=  0: position bookmark (also targets a 0-based line within the
    //               node's text)
    struct Bookmark
    {
        std::string id;
        int line = -1;
    };

    // Random 8-character identifier for tree nodes (matches the HTML app).
    std::string NewId();
}
