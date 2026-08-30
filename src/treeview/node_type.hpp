#pragma once

#include <string>

namespace terminadventure::treeview
{

    // Determines which UI renders a node. Each node's `type` selects the
    // component that displays it; EDITOR is the classic text-editor view.
    enum class NodeType
    {
        EDITOR,
        GAME,
        ROLLER,
        PLAYERS,
        ENEMIES,
    };

    inline const char* NodeTypeToString(NodeType t)
    {
        switch (t)
        {
            case NodeType::EDITOR:   return "EDITOR";
            case NodeType::GAME:     return "GAME";
            case NodeType::ROLLER:   return "ROLLER";
            case NodeType::PLAYERS:  return "PLAYERS";
            case NodeType::ENEMIES:  return "ENEMIES";
        }
        return "EDITOR";
    }

    inline NodeType NodeTypeFromString(const std::string& s)
    {
        if (s == "EDITOR")   return NodeType::EDITOR;
        if (s == "GAME")     return NodeType::GAME;
        if (s == "ROLLER")   return NodeType::ROLLER;
        if (s == "PLAYERS")  return NodeType::PLAYERS;
        if (s == "ENEMIES")  return NodeType::ENEMIES;
        return NodeType::EDITOR;
    }
}
