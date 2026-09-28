#pragma once

#include <string>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include "../screen/screen.hpp"

namespace terminadventure::command
{
    ftxui::Element process(const std::string& input, ftxui::ScreenInteractive& screen, terminadventure::screen::Type screenType);
    ftxui::Component createCommandInput(std::string& command_line, ftxui::ScreenInteractive& screen, int& selected_screen);
}
