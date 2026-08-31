#pragma once

#include <memory>
#include <cstddef>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace terminadventure::recent
{
    // `force` is a shared flag: when the dialog next opens, if `*force` is set
    // it starts in force-open mode (unsaved changes discarded) and the flag is
    // cleared. Lets a `recents_force` op pre-arm the dialog.
    ftxui::Component MakeRecentDialog(std::shared_ptr<EditorState> state, bool* show,
                                      std::shared_ptr<bool> force = nullptr);

    std::size_t PruneRecentFiles(std::vector<std::string>& recent_files);
}
