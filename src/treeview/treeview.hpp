#pragma once

#include <memory>
#include <string>
#include <vector>
#include "../app_state/app_state.hpp"
#include "treenode.hpp"
#include <ftxui/component/component.hpp>

namespace terminadventure::treeview
{
    ftxui::Component MakeTreeView(std::shared_ptr<AppState> state);
}
