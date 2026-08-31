#pragma once

#include <memory>
#include <string>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace terminadventure::help
{
    struct HelpEntry
    {
        std::string mode;
        std::string key;
        std::string command;
        std::string op;
        std::string description;
        std::string line;   // the diagram line shown in the dialog
    };

    // Build the key-binding help dialog. Reads its entries from `config_path`
    // (the same commands.conf format the loader uses). The dialog is shown
    // while *show is true; Escape hides it. While shown, it consumes every
    // event so no app key bindings fire.
    ftxui::Component MakeHelpDialog(std::shared_ptr<EditorState> state,
                                    const std::string& config_path,
                                    bool* show);
}
