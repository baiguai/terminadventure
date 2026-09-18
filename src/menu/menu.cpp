#include "./menu.hpp"
#include <vector>
#include <string>

namespace terminadventure::menu
{
    ftxui::Component createLeftMenu(int& selected_index)
    {
        using namespace ftxui;

        static const std::vector<std::string> entries =
        {
            "GM Tools",
            "Dice Roller"
        };

        return Menu(&entries, &selected_index);
    }
}
