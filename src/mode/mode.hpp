#pragma once

#include <string>

enum class Mode
{
    TREE,
    NORMAL,
    INSERT,
    VISUAL,
    VISUAL_LINE,
    VISUAL_BLOCK,
    COMMAND
};

inline std::string ModeName(Mode m)
{
    switch(m)
    {
        case Mode::TREE:            return "TREE";
        case Mode::NORMAL:          return "NORMAL";
        case Mode::INSERT:          return "INSERT";
        case Mode::VISUAL:          return "VISUAL";
        case Mode::COMMAND:         return "COMMAND";
        case Mode::VISUAL_LINE:     return "VISUAL LINE";
        case Mode::VISUAL_BLOCK:    return "VISUAL BLOCK";
    }
    return "???";
}
