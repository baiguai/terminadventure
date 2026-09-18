#include "./screen.hpp"

namespace terminadventure::screen
{
    ftxui::Element render(Type screen)
    {
        using namespace ftxui;

        switch (screen)
        {
            case Type::GmTools:
                return text("GM Tools Content");
            default:
                return text("Main Content Area");
        }
    }
}
