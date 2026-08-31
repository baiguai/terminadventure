#pragma once

#include <memory>
#include "editor_state.hpp"
#include <ftxui/component/component.hpp>

namespace terminadventure::editor {

ftxui::Component MakeEditor(std::shared_ptr<EditorState> state);

}
