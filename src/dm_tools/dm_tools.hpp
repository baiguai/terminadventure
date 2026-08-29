#pragma once

#include <memory>
#include "../app_state/app_state.hpp"
#include <ftxui/component/component.hpp>

namespace terminadventure::dm_tools
{
    ftxui::Component MakeDmTools(std::shared_ptr<AppState> state);
}
