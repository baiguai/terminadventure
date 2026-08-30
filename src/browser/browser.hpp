#pragma once

#include <memory>
#include <string>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace terminadventure::browser
{
    // Build the file-browser dialog. While *show is true it consumes every
    // event, so no app key bindings fire. The treeview sets
    // `state->browser_start_dir` and `state->browser_pick` before showing it
    // via a path command that received a directory argument (open/saveas/
    // import_html/export_html). Keys: j/k (or ArrowDown/ArrowUp) move the
    // selection, h (or Backspace) goes up one level, l or Enter enters a
    // directory, Enter picks the selected file (l does nothing on a file),
    // gg jumps to the first row, G to the last, / opens a case-insensitive
    // filter field that narrows the list as you type, Escape cancels.
    // While the filter field is open, Enter keeps the filtered list and hides
    // the field; Escape clears the filter and closes the field. Escape with a
    // filter applied (field hidden) clears the filter; only Escape with no
    // filter cancels the dialog. Picking a file invokes
    // `state->browser_pick` with its full path and closes the dialog.
    // For save/export-style commands the treeview also sets
    // `state->browser_command` to the invoking command name ("saveas"/"X");
    // then Enter on a directory closes the dialog and reopens the command
    // line prefilled with `:command <folder>/` so a filename can be typed
    // (l still navigates into the directory). For open/import-style commands
    // the field stays empty and Enter on a directory navigates as usual.
    ftxui::Component MakeFileBrowserDialog(std::shared_ptr<EditorState> state, bool* show);
}
